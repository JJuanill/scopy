#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QLabel>
#include <QFileDialog>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QTranslator>
#include <QOpenGLFunctions>
#include <browsemenu.h>

#include "logging_categories.h"
#include "qmessagebox.h"
#include "scopymainwindow.h"
#include "animationmanager.h"

#include "scanbuttoncontroller.h"
#include "ui_scopymainwindow.h"
#include "scopyhomepage.h"
#include "scopyaboutpage.h"
#include "scopypreferencespage.h"
#include "device.h"

#include <gui/restartdialog.h>
#include "application_restarter.h"
#include "pluginbase/preferences.h"
#include "pluginbase/scopyjs.h"
#include "iioutil/connectionprovider.h"
#include "pluginbase/messagebroker.h"
#include "scopy-core_config.h"
#include "pluginbase/statusbarmanager.h"
#include "scopytitlemanager.h"
#include <common/scopyconfig.h>
#include <translationsrepository.h>
#include <libsigrokdecode/libsigrokdecode.h>
#include <stylehelper.h>
#include <scopymainwindow_api.h>
#include <QVersionNumber>
#include <iioutil/iiounits.h>

using namespace scopy;
using namespace scopy::gui;

Q_LOGGING_CATEGORY(CAT_SCOPY, "Scopy")

ScopyMainWindow::ScopyMainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::ScopyMainWindow)
	, m_glLoader(nullptr)
{
	QElapsedTimer timer;
	timer.start();
	ui->setupUi(this);

	ScopyTitleManager::setMainWindow(this);
	ScopyTitleManager::setApplicationName("Scopy");
	ScopyTitleManager::setScopyVersion("v" + QString(scopy::config::version()));
	ScopyTitleManager::setGitHash(QString(SCOPY_VERSION_GIT));

	StyleHelper::GetInstance()->initColorMap();
	IIOUnitsManager::GetInstance();
	setAttribute(Qt::WA_QuitOnClose, true);
	initPythonWIN32();
	initStatusBar();
	initPreferences();

	ConnectionProvider::GetInstance();
	MessageBroker::GetInstance();

	// get the version document
	auto vc = VersionChecker::GetInstance(); // get VersionCache instance
	vc->subscribe(this,
		      &ScopyMainWindow::receiveVersionDocument); // 'subscribe' to receive the version QJsonDocument

	auto ts = ui->wsToolStack;

	////////
	BrowseMenu *browseMenu = new BrowseMenu(ui->wToolBrowser);
	ui->wToolBrowser->layout()->setMargin(0);
	ui->wToolBrowser->layout()->addWidget(browseMenu);
	QString toolBrowserStyle = QString(R"css( 
								QWidget#wToolBrowser{
									border-right: 1px solid &&borderColor&&; 
								}
							)css");
	toolBrowserStyle.replace(QString("&&borderColor&&"), StyleHelper::getColor("Dark-HMC-Color-Layout-Divider-Default"));
	ui->wToolBrowser->setStyleSheet(toolBrowserStyle);

	connect(browseMenu, &BrowseMenu::requestTool, ts, &ToolStack::show, Qt::QueuedConnection);
	connect(browseMenu, SIGNAL(requestLoad()), this, SLOT(load()));
	connect(browseMenu, SIGNAL(requestSave()), this, SLOT(save()));
	connect(browseMenu, &BrowseMenu::collapsed, this, [this](bool coll) {
		if(coll) {
			ui->animHolder->setAnimMin(40);
		} else {
			ui->animHolder->setAnimMax(230);
		}
		ui->animHolder->toggleMenu(!coll);
	});
	////////

	scanTask = new IIOScanTask(this);
	scanTask->setScanParams("usb");
	scanCycle = new CyclicalTask(scanTask, this);
	scc = new ScannedIIOContextCollector(this);
	pr = new PluginRepository(this);
	loadPluginsFromRepository(pr);

	PluginManager *pm = pr->getPluginManager();

	initAboutPage(pm);
	initPreferencesPage(pm);
	initTranslations();

	hp = new ScopyHomePage(this, pm);
	ScanButtonController *sbc = new ScanButtonController(scanCycle, hp->scanControlBtn(), this);

	dm = new DeviceManager(pm, this);
	dm->setExclusive(false);

	dtm = new DetachedToolWindowManager(this);
	m_toolMenuManager = new ToolMenuManager(ts, dtm, browseMenu->toolMenu(), this);

	ts->add("home", hp);
	ts->add("about", about);
	ts->add("preferences", prefPage);

	connect(scanTask, &IIOScanTask::scanFinished, scc, &ScannedIIOContextCollector::update, Qt::QueuedConnection);

	connect(scc, SIGNAL(foundDevice(QString, QString)), dm, SLOT(createDevice(QString, QString)));
	connect(scc, SIGNAL(lostDevice(QString, QString)), dm, SLOT(removeDevice(QString, QString)));

	connect(hp, SIGNAL(requestDevice(QString)), this, SLOT(requestTools(QString)));

	connect(dm, SIGNAL(deviceAdded(QString, Device *)), this, SLOT(addDeviceToUi(QString, Device *)));

	connect(dm, SIGNAL(deviceRemoveStarted(QString, Device *)), scc, SLOT(removeDevice(QString, Device *)));
	connect(dm, SIGNAL(deviceRemoveStarted(QString, Device *)), this, SLOT(removeDeviceFromUi(QString)));

	connect(dm, &DeviceManager::connectionStarted, sbc, &ScanButtonController::stopScan);
	connect(dm, &DeviceManager::connectionFinished, sbc, &ScanButtonController::startScan);

	if(dm->getExclusive()) {
		// only for device manager exclusive mode - stop scan on connect
		connect(dm, SIGNAL(deviceConnected(QString, Device *)), sbc, SLOT(stopScan()));
		connect(dm, SIGNAL(deviceDisconnected(QString, Device *)), sbc, SLOT(startScan()));
	}

	connect(dm, SIGNAL(deviceConnected(QString, Device *)), scc, SLOT(lock(QString, Device *)));
	connect(dm, SIGNAL(deviceConnected(QString, Device *)), hp, SLOT(connectDevice(QString)));
	connect(dm, SIGNAL(deviceDisconnected(QString, Device *)), scc, SLOT(unlock(QString, Device *)));
	connect(dm, SIGNAL(deviceDisconnected(QString, Device *)), hp, SLOT(disconnectDevice(QString)));

	connect(dm, SIGNAL(requestDevice(QString)), hp, SLOT(viewDevice(QString)));
	sbc->startScan();

	connect(dm, &DeviceManager::deviceChangedToolList, m_toolMenuManager, &ToolMenuManager::changeToolListContents);
	connect(dm, SIGNAL(deviceConnected(QString, Device *)), m_toolMenuManager, SLOT(deviceConnected(QString)));
	connect(dm, SIGNAL(deviceDisconnected(QString, Device *)), m_toolMenuManager,
		SLOT(deviceDisconnected(QString)));
	connect(dm, &DeviceManager::requestTool, m_toolMenuManager, &ToolMenuManager::showMenuItem);
	connect(m_toolMenuManager, &ToolMenuManager::requestToolSelect, ts, &ToolStack::show);
	connect(m_toolMenuManager, &ToolMenuManager::requestToolSelect, dtm, &DetachedToolWindowManager::show);
	connect(hp, &ScopyHomePage::displayNameChanged, m_toolMenuManager, &ToolMenuManager::onDisplayNameChanged);

	connect(hp, &ScopyHomePage::newDeviceAvailable, dm, &DeviceManager::addDevice);

	initApi();
#ifdef SCOPY_DEV_MODE
	// this is an example of how autoconnect is done

	//	 auto id = api->addDevice("ip:127.0.0.1", "m2k");
	//	 auto id = api->addDevice("ip:10.48.65.163", "iio");
	auto id = api->addDevice("ip:192.168.2.1", "iio");
	//	 auto id = api->addDevice("", "test");

	api->connectDevice(id);
	// api->switchTool(id, "Time");
#endif

	qInfo(CAT_BENCHMARK) << "ScopyMainWindow constructor took: " << timer.elapsed() << "ms";
}

void ScopyMainWindow::initStatusBar()
{
	// clear all margin, except the bottom one, to make room the status bar
	statusBar = new ScopyStatusBar(this);
	ui->mainWidget->layout()->addWidget(statusBar);
}

void ScopyMainWindow::save()
{
	QString selectedFilter;
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save"), "", "", &selectedFilter);
	save(fileName);
	ScopyTitleManager::setIniFileName(fileName);
}

void ScopyMainWindow::load()
{
	QString selectedFilter;
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), "", "", &selectedFilter);
	load(fileName);
	ScopyTitleManager::setIniFileName(fileName);
}

void ScopyMainWindow::save(QString file)
{
	QSettings s(file, QSettings::Format::IniFormat);
	dm->save(s);
	ScopyTitleManager::setIniFileName(file);
}

void ScopyMainWindow::load(QString file)
{
	QSettings s(file, QSettings::Format::IniFormat);
	dm->load(s);
	ScopyTitleManager::setIniFileName(file);
}

void ScopyMainWindow::closeEvent(QCloseEvent *event) { dm->disconnectAll(); }

void ScopyMainWindow::requestTools(QString id) { m_toolMenuManager->showMenuItem(id); }

ScopyMainWindow::~ScopyMainWindow()
{

	scanCycle->stop();
	delete ui;
}

void ScopyMainWindow::initAboutPage(PluginManager *pm)
{
	QElapsedTimer timer;
	timer.start();
	about = new ScopyAboutPage(this);
	if(!pm)
		return;
	QList<Plugin *> plugin = pm->getOriginalPlugins();
	for(Plugin *p : plugin) {
		QString content = p->about();
		if(!content.isEmpty()) {
			about->addHorizontalTab(about->buildPage(content), p->name());
		}
	}
	qInfo(CAT_BENCHMARK) << " Init about page took: " << timer.elapsed() << "ms";
}

void ScopyMainWindow::initPreferencesPage(PluginManager *pm)
{
	prefPage = new ScopyPreferencesPage(this);
	if(!pm)
		return;

	QList<Plugin *> plugin = pm->getOriginalPlugins();
	for(Plugin *p : plugin) {
		p->initPreferences();
		if(p->loadPreferencesPage()) {
			prefPage->addHorizontalTab(p->preferencesPage(), p->name());
		}
	}
}

void ScopyMainWindow::initTranslations()
{
	TranslationsRepository *t = TranslationsRepository::GetInstance();
	t->loadTranslations(Preferences::GetInstance()->get("general_language").toString());
}

void ScopyMainWindow::initPreferences()
{
	QElapsedTimer timer;
	timer.start();
	QString preferencesPath = scopy::config::preferencesFolderPath() + "/preferences.ini";
	Preferences *p = Preferences::GetInstance();
	p->setPreferencesFilename(preferencesPath);
	p->load();
	p->init("general_first_run", true);
	p->init("general_save_session", true);
	p->init("general_save_attached", true);
	p->init("general_doubleclick_attach", true);
#if defined(__arm__)
	p->init("general_use_opengl", false);
#else
	p->init("general_use_opengl", true);
#endif
	p->init("general_use_animations", true);
	p->init("general_theme", "default");
	p->init("general_language", "en");
	p->init("show_grid", true);
	p->init("show_graticule", false);
	p->init("iiowidgets_use_lazy_loading", true);
	p->init("general_plot_target_fps", "60");
	p->init("general_show_plot_fps", true);
	p->init("general_use_native_dialogs", false);
	p->init("general_additional_plugin_path", "");
	p->init("general_load_decoders", true);
	p->init("general_doubleclick_ctrl_opens_menu", true);
	p->init("general_check_online_version", false);
	p->init("general_show_status_bar", true);

	connect(p, SIGNAL(preferenceChanged(QString, QVariant)), this, SLOT(handlePreferences(QString, QVariant)));

	if(p->get("general_use_opengl").toBool()) {
		m_glLoader = new QOpenGLWidget(this);
	}
	if(p->get("general_load_decoders").toBool()) {
		loadDecoders();
	}
	if(p->get("general_show_status_bar").toBool()) {
		StatusBarManager::GetInstance()->setEnabled(true);
	}
	if(p->get("general_first_run").toBool()) {
		license = new LicenseOverlay(this);
		auto versionCheckInfo = new VersionCheckMessage(this);

		StatusBarManager::pushWidget(versionCheckInfo, "Should Scopy check for online versions?");

		QMetaObject::invokeMethod(license, &LicenseOverlay::showOverlay, Qt::QueuedConnection);
	}
	QString theme = p->get("general_theme").toString();
	QString themeName = "scopy-" + theme;
	QIcon::setThemeName(themeName);
	QIcon::setThemeSearchPaths({":/gui/icons/" + themeName});
	qInfo(CAT_BENCHMARK) << "Init preferences took: " << timer.elapsed() << "ms";
}

void ScopyMainWindow::loadOpenGL()
{
	bool disablePref = false;
	QOpenGLContext *ct = QOpenGLContext::currentContext();
	if(ct) {
		QOpenGLFunctions *glFuncs = ct->functions();
		bool glCtxValid = ct->isValid();
		QString glVersion = QString((const char *)(glFuncs->glGetString(GL_VERSION)));
		qInfo(CAT_BENCHMARK) << "GL_VENDOR " << reinterpret_cast<const char *>(glFuncs->glGetString(GL_VENDOR));
		qInfo(CAT_BENCHMARK) << "GL_RENDERER "
				     << reinterpret_cast<const char *>(glFuncs->glGetString(GL_RENDERER));
		qInfo(CAT_BENCHMARK) << "GL_VERSION " << glVersion;
		qInfo(CAT_BENCHMARK) << "GL_EXTENSIONS "
				     << reinterpret_cast<const char *>(glFuncs->glGetString(GL_EXTENSIONS));
		qInfo(CAT_BENCHMARK) << "QOpenGlContext valid: " << glCtxValid;
		if(!glCtxValid || glVersion.compare("2.0.0", Qt::CaseInsensitive) < 0) {
			disablePref = true;
		}
	} else {
		qInfo(CAT_BENCHMARK) << "QOpenGlContext is invalid";
		disablePref = true;
	}

	qInfo(CAT_BENCHMARK) << "OpenGL load status: " << !disablePref;
	if(disablePref) {
		Preferences::GetInstance()->set("general_use_opengl", false);
		Preferences::GetInstance()->save();
		RestartDialog *restarter = new RestartDialog(this);
		restarter->setDescription(
			"Scopy uses OpenGL for high performance plot rendering. Valid OpenGL context (>v2.0.0) not "
			"detected.\n"
			"Restarting will set Scopy rendering mode to software. This option can be changed from the "
			"Preferences "
			"menu.\n"
			"Please visit the <a "
			"href=https://wiki.analog.com/university/tools/m2k/scopy-troubleshooting> Wiki "
			"Analog page</a> for troubleshooting.");
		connect(restarter, &RestartDialog::restartButtonClicked, [=] {
			ApplicationRestarter::triggerRestart();
			restarter->deleteLater();
		});
		QMetaObject::invokeMethod(restarter, &RestartDialog::showDialog, Qt::QueuedConnection);
	}

	delete m_glLoader;
	m_glLoader = nullptr;
}

void ScopyMainWindow::loadPluginsFromRepository(PluginRepository *pr)
{

	QElapsedTimer timer;
	timer.start();
	// Check the local build plugins folder first
	// Check if directory exists and it's not empty
	QDir pathDir(scopy::config::localPluginFolderPath());

	if(pathDir.exists() && pathDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).count() != 0) {
		pr->init(scopy::config::localPluginFolderPath());
	} else {
		pr->init(scopy::config::defaultPluginFolderPath());
	}

#ifndef Q_OS_ANDROID
	QString pluginAdditionalPath = Preferences::GetInstance()->get("general_additional_plugin_path").toString();
	if(!pluginAdditionalPath.isEmpty()) {
		pr->init(pluginAdditionalPath);
	}
#endif

	qInfo(CAT_BENCHMARK) << "Loading the plugins from the repository took: " << timer.elapsed() << "ms";
}

void ScopyMainWindow::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	Preferences *p = Preferences::GetInstance();
	if(p->get("general_use_opengl").toBool() && m_glLoader) {
		loadOpenGL();
	}
}

void ScopyMainWindow::handlePreferences(QString str, QVariant val)
{
	Preferences *p = Preferences::GetInstance();

	if(str == "general_use_opengl") {
		Q_EMIT p->restartRequired();

	} else if(str == "general_use_animations") {
		AnimationManager::getInstance().toggleAnimations(val.toBool());

	} else if(str == "general_theme") {
		Q_EMIT p->restartRequired();

	} else if(str == "general_language") {
		Q_EMIT p->restartRequired();
	} else if(str == "general_show_status_bar") {
		StatusBarManager::GetInstance()->setEnabled(val.toBool());
	} else if(str == "plugins_use_debugger_v2") {
		Q_EMIT p->restartRequired();
	}
}

void ScopyMainWindow::initPythonWIN32()
{
#ifdef WIN32
	QString pythonhome;
	QString pythonpath;

	pythonpath += QCoreApplication::applicationDirPath() + "\\" + PYTHON_VERSION + ";";
	pythonpath += QCoreApplication::applicationDirPath() + "\\" + PYTHON_VERSION + "\\plat-win;";
	pythonpath += QCoreApplication::applicationDirPath() + "\\" + PYTHON_VERSION + "\\lib-dynload;";
	pythonpath += QCoreApplication::applicationDirPath() + "\\" + PYTHON_VERSION + "\\site-packages;";
	QString scopypythonpath = qgetenv("SCOPY_PYTHONPATH");
	pythonpath += scopypythonpath;

#ifdef SCOPY_DEV_MODE
	pythonhome += QString(BUILD_PYTHON_LIBRARY_DIRS) + ";";
	pythonpath += QString(BUILD_PYTHON_LIBRARY_DIRS) + ";";
	pythonpath += QString(BUILD_PYTHON_LIBRARY_DIRS) + "\\plat-win;";
	pythonpath += QString(BUILD_PYTHON_LIBRARY_DIRS) + "\\lib-dynload;";
	pythonpath += QString(BUILD_PYTHON_LIBRARY_DIRS) + "\\site-packages;";
#endif

	qputenv("PYTHONHOME", pythonhome.toLocal8Bit());
	qputenv("PYTHONPATH", pythonpath.toLocal8Bit());

	qInfo(CAT_SCOPY) << "SCOPY_PYTHONPATH: " << scopypythonpath;
	qInfo(CAT_SCOPY) << "PYTHONHOME: " << qgetenv("PYTHONHOME");
	qInfo(CAT_SCOPY) << "PYTHONPATH: " << qgetenv("PYTHONPATH");
#endif
}

void ScopyMainWindow::loadDecoders()
{
	QElapsedTimer timer;
	timer.start();
#if defined(WITH_SIGROK) && defined(WITH_PYTHON)
#if defined __APPLE__
	QString path = QCoreApplication::applicationDirPath() + "/decoders";
#elif defined(__appimage__)
	QString path = QCoreApplication::applicationDirPath() + "/../lib/decoders";
#else
	QString path = "decoders";
#endif

	bool success = true;
	static bool srd_loaded = false;
	if(srd_loaded) {
		srd_exit();
	}

	if(srd_init(path.toStdString().c_str()) != SRD_OK) {
		qInfo(CAT_SCOPY) << "ERROR: libsigrokdecode init failed.";
		success = false;
	} else {
		srd_loaded = true;
		/* Load the protocol decoders */
		srd_decoder_load_all();
		auto decoder = srd_decoder_get_by_id("parallel");

		if(decoder == nullptr) {
			success = false;
			qInfo(CAT_SCOPY) << "ERROR: libsigrokdecode load the protocol decoders failed.";
		}
	}

	if(!success) {
		QMessageBox error(this);
		error.setText(
			tr("ERROR: There was a problem initializing libsigrokdecode. Some features may be missing"));
		error.exec();
	}
#else
	qInfo(CAT_SCOPY) << "Python or libsigrokdecode are disabled, can't load decoders";
#endif
	qInfo(CAT_BENCHMARK) << "Loading the decoders took: " << timer.elapsed() << "ms";
}

void ScopyMainWindow::initApi()
{
	api = new ScopyMainWindow_API(this);
	ScopyJS *js = ScopyJS::GetInstance();
	api->setObjectName("scopy");
	js->registerApi(api);
}

void ScopyMainWindow::addDeviceToUi(QString id, Device *d)
{
	m_toolMenuManager->addMenuItem(id, d->displayName(), d->toolList());
	hp->addDevice(id, d);
}

void ScopyMainWindow::removeDeviceFromUi(QString id)
{
	m_toolMenuManager->removeMenuItem(id);
	hp->removeDevice(id);
}

void ScopyMainWindow::receiveVersionDocument(QJsonDocument document)
{
	QJsonValue scopyJson = document["scopy"];
	if(scopyJson.isNull()) {
		qWarning(CAT_SCOPY) << "Could not find the entry \"scopy\" in the json document";
		return;
	}

	QJsonValue scopyVersion = scopyJson["version"];
	if(scopyVersion.isNull()) {
		qWarning(CAT_SCOPY)
			<< R"(Could not find the entry "version" in the "scopy" entry of the json document)";
		return;
	}

	QVersionNumber currentScopyVersion = QVersionNumber::fromString(SCOPY_VERSION).normalized();
	QVersionNumber upstreamScopyVersion =
		QVersionNumber::fromString(scopyVersion.toString().remove(0, 1)).normalized();

	if(upstreamScopyVersion > currentScopyVersion) {
		StatusBarManager::pushMessage(
			"Your Scopy version of outdated. Please consider updating it. The newest version is " +
				upstreamScopyVersion.toString(),
			10000); // 10 sec
	}

	qInfo(CAT_SCOPY) << "The upstream scopy version is" << upstreamScopyVersion << "and the current one is"
			 << currentScopyVersion;
}

#include "moc_scopymainwindow.cpp"
