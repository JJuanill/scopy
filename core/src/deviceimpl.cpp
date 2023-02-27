#include "deviceimpl.h"
#include "qboxlayout.h"
#include "qpushbutton.h"
#include <QLabel>
#include <QTextBrowser>
#include <QLoggingCategory>
#include <QDebug>
#include <QPicture>

Q_LOGGING_CATEGORY(CAT_DEVICEIMPL, "Device")

namespace adiscope {
DeviceImpl::DeviceImpl(QString uri, PluginManager *p, QObject *parent)
	: QObject{parent},
	  m_uri(uri),
	  p(p)
{
	qDebug(CAT_DEVICEIMPL)<< m_uri <<"ctor";
}

void DeviceImpl::loadCompatiblePlugins()
{
	plugins = p->getCompatiblePlugins(m_uri,"",this);
}


void DeviceImpl::loadPlugins() {

	loadCompatiblePlugins();
	for(auto &p : plugins) {
		p->preload();
	}

	loadName();
	loadIcons();
	loadPages();
	loadToolList();

	for(auto &p : plugins) {
		connect(dynamic_cast<QObject*>(p),SIGNAL(disconnectDevice()),this,SLOT(disconnectDev()));
		connect(dynamic_cast<QObject*>(p),SIGNAL(toolListChanged()),this,SIGNAL(toolListChanged()));
		connect(dynamic_cast<QObject*>(p),SIGNAL(restartDevice()),this,SIGNAL(requestedRestart()));
		connect(dynamic_cast<QObject*>(p),SIGNAL(requestTool(QString)),this,SIGNAL(requestTool(QString)));
		p->postload();
	}
}

void DeviceImpl::unloadPlugins() {
	QList<Plugin*>::const_iterator pI = plugins.constEnd();
	while(pI != plugins.constBegin()) {
		--pI;
		disconnect(dynamic_cast<QObject*>(p),SIGNAL(disconnectDevice()),this,SLOT(disconnectDev()));
		disconnect(dynamic_cast<QObject*>(*pI),SIGNAL(toolListChanged()),this,SIGNAL(toolListChanged()));
		disconnect(dynamic_cast<QObject*>(*pI),SIGNAL(restartDevice()),this,SIGNAL(requestedRestart()));
		disconnect(dynamic_cast<QObject*>(*pI),SIGNAL(requestTool(QString)),this,SIGNAL(requestTool(QString)));

		(*pI)->unload();
		delete (*pI);
	}
	plugins.clear();
}

void DeviceImpl::loadName() {
	if(plugins.count())
		m_name = plugins[0]->name();
	else
		m_name = "NO_PLUGIN";
}

void DeviceImpl::loadIcons() {
	m_icon = new QWidget();
	m_icon->setFixedHeight(100);
	m_icon->setFixedWidth(100);
	new QHBoxLayout(m_icon);

	for( auto &p : plugins) {
		if(p->loadIcon()) {
			m_icon->layout()->addWidget(p->icon());
			return;
		}
	}

	new QLabel("No PLUGIN",m_icon);
}

void DeviceImpl::loadPages() {
	m_page = new QWidget();
	auto m_pagelayout = new QVBoxLayout(m_page);
	connbtn = new QPushButton("connect", m_page);
	connbtn->setProperty("blue_button",true);
	m_pagelayout->addWidget(connbtn);
	discbtn = new QPushButton("disconnect", m_page);
	discbtn->setProperty("blue_button",true);
	m_pagelayout->addWidget(discbtn);
	discbtn->setVisible(false);

	connect(connbtn,SIGNAL(clicked()),this,SLOT(connectDev()));
	connect(discbtn,SIGNAL(clicked()),this,SLOT(disconnectDev()));

	for(auto &&p : plugins) {
		if(p->loadPage()) {
			m_page->layout()->addWidget(p->page());
		}
	}
}

void DeviceImpl::loadToolList() {
	for(auto &&p : plugins) {
		p->loadToolList();
	}
}


void DeviceImpl::showPage() {
	for(auto &&p : plugins)
		p->showPageCallback();

}

void DeviceImpl::hidePage() {
	for(auto &&p : plugins)
		p->hidePageCallback();

}

void DeviceImpl::connectDev() {
	discbtn->show();
	connbtn->hide();
	for(auto &&p : plugins)
		p->onConnect();
	Q_EMIT connected();
}

void DeviceImpl::disconnectDev() {
	discbtn->hide();
	connbtn->show();
	for(auto &&p : plugins)
		p->onDisconnect();
	Q_EMIT disconnected();
}

DeviceImpl::~DeviceImpl() {

	qDebug(CAT_DEVICEIMPL)<< m_uri <<"dtor";
}

QString DeviceImpl::name()
{
	return m_name;
}

QString DeviceImpl::uri()
{
	return m_uri;
}

QWidget *DeviceImpl::icon()
{
	return m_icon;
}

QWidget *DeviceImpl::page()
{
	return m_page;
}

QList<ToolMenuEntry*> DeviceImpl::toolList()
{
	static int i;
	QList<ToolMenuEntry*> ret;

	for(auto &&p : plugins) {
		ret.append(p->toolList());
	}
	return ret;
}




}
