#ifndef TOOLBUILDER_HPP
#define TOOLBUILDER_HPP

#include "tool_view.hpp"

namespace adiscope::gui {

struct ToolViewRecipe
{
	QString helpBtnUrl{""};
	bool hasHelpBtn{true};
	bool hasPrintBtn{false};
	bool hasGroupBtn{false};

	bool hasRunBtn{false};
	bool hasSingleBtn{false};

	bool hasPairSettingsBtn{false};

	bool hasChannels{false};
	ChannelsPositionEnum channelsPosition{ChannelsPositionEnum::HORIZONTAL};
	bool hasHeader{true};
};

class ToolViewBuilder
{
public:
	explicit ToolViewBuilder(const ToolViewRecipe& recipe, ChannelManager* channelManager = nullptr, QWidget* parent = nullptr);

	ToolView* build();

private:
	ToolView* m_toolView;
};
} // namespace adiscope::gui

#endif // TOOLBUILDER_HPP