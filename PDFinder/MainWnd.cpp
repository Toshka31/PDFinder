#include "stdafx.h"
#include "tmp\moc_MainWnd.cpp"
#include "tmp\qrc_icons.cpp"

// TODO result saving
// TODO deep search

MainWnd::MainWnd()
{
	Q_INIT_RESOURCE(icons);

	QWidget *centralwidget = new QWidget(this);
	setCentralWidget(centralwidget);

	m_menu = new QMenuBar(this);
	m_menu->addAction(tr("Save"), this, &MainWnd::saveResult);
	m_menu->addAction(tr("Load"), this, &MainWnd::loadResult);
	setMenuBar(m_menu);

	QVBoxLayout *ltMain = new QVBoxLayout(centralwidget);

	QGridLayout *lt_info = new QGridLayout(this);

	//QHBoxLayout *ltPath = new QHBoxLayout(this);
	QLabel *lblPath = new QLabel("Path", this);
	m_le_root_path = new QLineEdit(this);
	m_le_root_path->setReadOnly(true);
	m_btnBrowse = new QPushButton("browse", this);

	//QHBoxLayout *lbPhrase = new QHBoxLayout(this);
	QLabel *lblPhrase = new QLabel("Search for", this);
	m_le_find_phrase = new QLineEdit(this);

	m_chb_deep = new QCheckBox("deep search", this);

	m_btnSearch = new QPushButton("Search", this);

	m_lblResult = new QLabel("Result", this);

	m_progress = new QProgressBar(this);
	m_progress->setVisible(false);

	m_tree_result = new QTreeWidget(this);
	m_tree_result->setHeaderHidden(true);

	lt_info->addWidget(lblPath, 0, 0);
	lt_info->addWidget(m_le_root_path, 0, 1);
	lt_info->addWidget(m_btnBrowse, 0, 2);

	lt_info->addWidget(lblPhrase, 1, 0);
	lt_info->addWidget(m_le_find_phrase, 1, 1);
	lt_info->addWidget(m_chb_deep, 1, 2);

	//ltMain->addLayout(ltPath);
	//ltMain->addLayout(lbPhrase);
	ltMain->addLayout(lt_info);
	ltMain->addWidget(m_btnSearch);
	ltMain->addWidget(m_lblResult);
	ltMain->addWidget(m_progress);
	ltMain->addWidget(m_tree_result);

	centralwidget->setLayout(ltMain);

	connect(m_btnBrowse, &QPushButton::released, this, &MainWnd::browseDirectory);
	connect(m_btnSearch, &QPushButton::released, this, &MainWnd::startSearch);
	connect(m_tree_result, &QTreeWidget::itemDoubleClicked, this, &MainWnd::openDocument);

#ifdef _DEBUG
	m_le_find_phrase->setText("37");
	m_le_root_path->setText("C:/Work/UpWork/PDFinder");
#endif // DEBUG

}

MainWnd::~MainWnd()
{
	if (m_textsearch_watcher)
	{
		if (m_textsearch_watcher->isRunning())
		{
			m_textsearch_watcher->cancel();
			m_textsearch_watcher->waitForFinished();
		}
		delete m_textsearch_watcher;
	}
	if (m_filesearch_watcher)
	{
		if (m_filesearch_watcher->isRunning())
		{
			m_filesearch_watcher->cancel();
			m_filesearch_watcher->waitForFinished();
		}
		delete m_filesearch_watcher;
	}
}

QVector<QFileInfo> MainWnd::serachFiles()
{
	// search files
	QVector<QFileInfo> list_file;
	QDirIterator it(m_le_root_path->text(), QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		it.next();
		if (it.fileInfo().suffix() == "pdf")
		{
			list_file.push_back(it.fileInfo());
		}
	}
	return list_file;
}

QVector<QFileInfo> MainWnd::collectFiles()
{
	QVector<QFileInfo> list_file;
	QTreeWidgetItemIterator it(m_tree_result, QTreeWidgetItemIterator::HasChildren);
	while (*it) {
		QString abs_path = (*it)->data(0, Qt::UserRole).toString();
		if (!abs_path.isEmpty())
		{
			QFileInfo finfo(abs_path);
			list_file.push_back(finfo);
		}
		++it;
	}
	return list_file;
}

void MainWnd::browseDirectory()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	m_le_root_path->setText(dir);
}

void MainWnd::startSearch()
{
	m_lblResult->setText("Result");

	m_btnBrowse->setEnabled(false);
	m_btnSearch->setEnabled(false);
	m_le_find_phrase->setEnabled(false);
	m_menu->setEnabled(false);
	m_chb_deep->setEnabled(false);
	m_progress->setVisible(true);

	m_progress->setRange(0, 0);
	m_progress->setValue(1);

	if (m_filesearch_watcher)
		delete m_filesearch_watcher;
	m_filesearch_watcher = new QFutureWatcher<QVector<QFileInfo>>;
	connect(m_filesearch_watcher, SIGNAL(finished()), this, SLOT(searchFilesFinished()));
	QFuture<QVector<QFileInfo>> future;
	if (m_chb_deep->isChecked())
		future = QtConcurrent::run(this, &MainWnd::collectFiles);
	else
		future = QtConcurrent::run(this, &MainWnd::serachFiles);
	m_filesearch_watcher->setFuture(future);
}

void MainWnd::openDocument(QTreeWidgetItem * item, int column)
{
	qDebug() << "file:///" + item->data(0, Qt::UserRole).toString();
	QDesktopServices::openUrl(QUrl("file:///" + item->data(0, Qt::UserRole).toString(), QUrl::TolerantMode));
}

void MainWnd::searchFilesFinished()
{
	m_tree_result->clear();
	QVector<QFileInfo> list_file = m_filesearch_watcher->result();

	// search in files
	m_progress->setRange(0, list_file.size());
	m_progress->setValue(0);

	struct PDFProcessor
	{
		PDFProcessor(QString searchtext, QTreeWidget *tree, QProgressBar *bar)
			: m_searchtext(searchtext),
			m_tree(tree),
			m_bar(bar)
		{ }

		typedef QFileInfo result_type;

		QFileInfo operator()(const QFileInfo & file_path)
		{
			QProcess gzip;
			gzip.start("pdftotext.exe", QStringList() << file_path.absoluteFilePath());
			if (gzip.waitForFinished(600000))
			{
				QString txtPath = file_path.absolutePath() + "/" + file_path.completeBaseName() + ".txt";

				QList<QTreeWidgetItem*> children;

				QFile MyFile(txtPath);
				MyFile.open(QIODevice::ReadWrite);
				QTextStream in(&MyFile);
				QString line;
				do
				{
					line = in.readLine();
					if (line.contains(m_searchtext, Qt::CaseInsensitive))
					{
						QTreeWidgetItem *childitem = new QTreeWidgetItem();
						childitem->setText(0, line);
						children.push_back(childitem);
					}
				} while (!line.isNull());
				MyFile.close();

				if (!children.empty())
				{
					QTreeWidgetItem *item = new QTreeWidgetItem();
					item->setText(0, file_path.fileName() + " [" + QString::number(children.size()) + "]");
					//item->setText(1, QString::number(children.size()));
					item->setData(0, Qt::UserRole, file_path.absoluteFilePath());
					item->addChildren(children);
					m_tree->addTopLevelItem(item);
				}

				QFile::remove(txtPath);

// 				m_bar->blockSignals(true);
// 				mutex.lock();
				int value = m_bar->value() + 1;
				QMetaObject::invokeMethod(m_bar, "setValue", Qt::QueuedConnection, Q_ARG(int, value));
// 				m_bar->blockSignals(false);
// 				mutex.unlock();
			}
			return file_path;
		}

		QString m_searchtext;
		QTreeWidget *m_tree;
		QProgressBar *m_bar;
	};

	if (m_textsearch_watcher)
		delete m_textsearch_watcher;
	m_textsearch_watcher = new QFutureWatcher<void>;
	connect(m_textsearch_watcher, SIGNAL(finished()), this, SLOT(searchTextFinished()));
	QFuture<void> fut = QtConcurrent::mapped(list_file, PDFProcessor(m_le_find_phrase->text(), m_tree_result, m_progress));
	m_textsearch_watcher->setFuture(fut);
}

void MainWnd::searchTextFinished()
{
	m_btnBrowse->setEnabled(true);
	m_le_find_phrase->setEnabled(true);
	m_btnSearch->setEnabled(true);
	m_menu->setEnabled(true);
	m_chb_deep->setEnabled(true);
	m_progress->setVisible(false);

	int count_entries = 0;
	QTreeWidgetItemIterator it(m_tree_result, QTreeWidgetItemIterator::NoChildren);
	while (*it) {
		count_entries++;
		++it;
	}
	m_lblResult->setText("Result: [" + QString::number(m_tree_result->topLevelItemCount()) + " files\t" + QString::number(count_entries) + " entries]");
}

void MainWnd::saveResult()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save results"), "/result", tr("Data (*.dat)"));
	if (fileName.size())
	{
		// TODO: check if file exist and if yes ask to do replace

		QDomDocument doc("Results");
		QDomElement root = doc.createElement("Results");
		doc.appendChild(root);

		QDomElement tagPath = doc.createElement("SearchPath");
		QDomText tPath = doc.createTextNode(m_le_root_path->text());
		tagPath.appendChild(tPath);
		root.appendChild(tagPath);

		QDomElement tagPhrase = doc.createElement("SearchPhrase");
		QDomText tPhrase = doc.createTextNode(m_le_find_phrase->text());
		tagPhrase.appendChild(tPhrase);
		tagPhrase.setAttribute("deep_search", m_chb_deep->checkState());
		root.appendChild(tagPhrase);

		QDomElement tagFilesList = doc.createElement("FilesList");
		for (int indx = 0; indx < m_tree_result->topLevelItemCount(); ++indx)
		{
			QTreeWidgetItem *topItem = m_tree_result->topLevelItem(indx);
			QDomElement tagFile = doc.createElement("File");
			tagFile.setAttribute("path", topItem->data(0, Qt::UserRole).toString());
			tagFile.setAttribute("text", topItem->text(0));
			for (int indx_child = 0; indx_child < topItem->childCount(); ++indx_child)
			{
				QTreeWidgetItem *childItem = topItem->child(indx_child);
				QDomElement tagEntry = doc.createElement("Entry");
				QDomText tEntryText = doc.createTextNode(childItem->text(0));
				tagEntry.appendChild(tEntryText);
				tagFile.appendChild(tagEntry);
			}
			tagFilesList.appendChild(tagFile);
		}
		root.appendChild(tagFilesList);

		QString xml = doc.toString();

		QFile file(fileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream stream(&file);
		stream << xml;
		file.close();
	}
}

void MainWnd::loadResult()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open results"), "/", tr("Data (*.dat)"));
	QDomDocument doc("Results");
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, "Load error", "Can't open the file!", QMessageBox::Ok, QMessageBox::Ok);
		return;
	}
	if (!doc.setContent(&file)) {
		file.close();
		QMessageBox::warning(this, "Load error", "The file has incorrect format!", QMessageBox::Ok, QMessageBox::Ok);
		return;
	}
	file.close();

	m_tree_result->clear();

	QDomElement root = doc.documentElement();

	QDomElement tagPath = root.firstChildElement("SearchPath");
	if (tagPath.isNull())
	{
		QMessageBox::warning(this, "Load error", "The file has incorrect format!", QMessageBox::Ok ,QMessageBox::Ok);
		return;
	}
	m_le_root_path->setText(tagPath.text());

	QDomElement tagPhrase = root.firstChildElement("SearchPhrase");
	if (tagPhrase.isNull())
	{
		QMessageBox::warning(this, "Load error", "The file has incorrect format!", QMessageBox::Ok, QMessageBox::Ok);
		return;
	}
	m_le_find_phrase->setText(tagPhrase.text());

	QDomElement tagFileList = root.firstChildElement("FilesList");
	if (tagFileList.isNull())
	{
		QMessageBox::warning(this, "Load error", "The file has incorrect format!", QMessageBox::Ok, QMessageBox::Ok);
		return;
	}
	for (QDomElement tagFile = tagFileList.firstChildElement("File"); !tagFile.isNull(); tagFile = tagFile.nextSiblingElement("File"))
	{
		QTreeWidgetItem *topItem = new QTreeWidgetItem(m_tree_result);
		topItem->setText(0, tagFile.attribute("text"));
		topItem->setData(0, Qt::UserRole, tagFile.attribute("path"));
		for (QDomElement tagEntry = tagFile.firstChildElement("Entry"); !tagEntry.isNull(); tagEntry = tagEntry.nextSiblingElement("Entry"))
		{
			QTreeWidgetItem *childItem = new QTreeWidgetItem(topItem);
			childItem->setText(0, tagEntry.text());
		}
	}
}
