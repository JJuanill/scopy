/*
 * Copyright (c) 2024 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see https://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef TOOL_VIEW_HPP
#define TOOL_VIEW_HPP

#include "scopy-m2k-gui_export.h"
#include "channel_manager.hpp"
#include "channel_widget.hpp"
#include "customPushButton.h"
#include "custom_menu_button.hpp"
#include "generic_menu.hpp"
#include "linked_button.hpp"
#include "menu_anim.hpp"

#include <QButtonGroup>
#include <QDockWidget>
#include <QMap>
#include <QQueue>
#include <QStackedWidget>
#include <QWidget>

namespace Ui {
class ToolView;
}

namespace scopy {
namespace m2kgui {

class SCOPY_M2K_GUI_EXPORT ToolView : public QWidget
{
	friend class ToolViewBuilder;

	Q_OBJECT

public:
	explicit ToolView(QWidget *parent = nullptr);
	~ToolView();

	LinkedButton *getHelpBtn();
	void setHelpBtnVisible(bool visible);
	void setUrlHelpBtn(const QString &url);

	QPushButton *getPrintBtn();
	void setPrintBtnVisible(bool visible);

	QPushButton *getRunBtn();
	void setRunBtnVisible(bool visible);

	QPushButton *getSingleBtn();
	void setSingleBtnVisible(bool visible);

	QWidget *getTopExtraWidget();
	void setVisibleTopExtraWidget(bool visible);
	void addTopExtraWidget(QWidget *widget);

	QWidget *getBottomExtraWidget();
	void setVisibleBottomExtraWidget(bool visible);
	void addBottomExtraWidget(QWidget *widget);

	QWidget *getCentralWidget();
	QStackedWidget *getStackedWidget();

	CustomPushButton *getGeneralSettingsBtn();
	QPushButton *getSettingsBtn();
	void setPairSettingsVisible(bool visible);

	void setInstrumentNotesVisible(bool visible);

	void setFixedMenu(QWidget *menu, bool dockable);
	void setGeneralSettingsMenu(QWidget *menu, bool dockable);

	ChannelWidget *buildNewChannel(ChannelManager *channelManager, GenericMenu *menu, bool dockable, int chId,
				       bool deletable, bool simplified, QColor color, const QString &fullName,
				       const QString &shortName);
	void buildChannelGroup(ChannelManager *channelManager, ChannelWidget *mainChannal,
			       std::vector<ChannelWidget *> channelGroup);
	CustomMenuButton *buildNewInstrumentMenu(GenericMenu *menu, bool dockable, const QString &name,
						 bool checkBoxVisible = false, bool checkBoxChecked = false);

	void addFixedCentralWidget(QWidget *widget, int row = -1, int column = -1, int rowspan = -1,
				   int columnspan = -1);
	int addDockableCentralWidget(QWidget *widget, Qt::DockWidgetArea area, const QString &dockerName);
	QDockWidget *addDockableTabbedWidget(QWidget *plot, const QString &dockerName);
	int addFixedTabbedWidget(QWidget *widget, const QString &title, int plotId = -1, int row = -1, int column = -1,
				 int rowspan = -1, int columnspan = -1);

	void setWidgetVisibility(int widgetId, bool visible);
	bool isWidgetHidden(int widgetId);

	void setHeaderVisibility(bool visible);

	scopy::MenuHAnim *addMenuToStack();
	void setStackedWidget(QStackedWidget *sw);

	int getNewID();
	void addPlotInfoWidget(QWidget *widget);
	QWidget *getPlotInfoWidget();

private:
	void configureLastOpenedMenu();
	void buildChannelsContainer(ChannelManager *channelManager, ChannelsPositionEnum position);
	void toggleRightMenu(CustomPushButton *btn, bool checked);
	void settingsPanelUpdate(int id);
	void rightMenuFinished(bool opened);
	QDockWidget *createDetachableMenu(QWidget *menu, int &id);
	QDockWidget *createDockableWidget(QWidget *widget, const QString &dockerName);

public Q_SLOTS:
	void triggerRightMenuToggle(bool checked);
	void configureAddMathBtn(QWidget *menu, bool dockable);

Q_SIGNALS:
	void changeParent(QWidget *newParent);
	void channelDisabled(bool disabled);
	void instrumentMenuDisabled(bool disabled);

private:
	Ui::ToolView *m_ui;

	QButtonGroup m_group;
	QButtonGroup m_channelsGroup; // selected state of each channel

	QQueue<QPair<CustomPushButton *, bool>> m_menuButtonActions;
	QList<CustomPushButton *> m_menuOrder;

	int m_generalSettingsMenuId;

	QMainWindow *m_centralMainWindow;
	QList<QDockWidget *> m_docksList;
	QList<QWidget *> m_centralFixedWidgets;
	QMap<int, QWidget *> m_menuList;

	int m_nextMenuIndex;
	void initPlotInfoWidget();
};
} // namespace m2kgui
} // namespace scopy

#endif // TOOL_VIEW_HPP
