/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "brave/app/brave_command_ids.h"
#include "brave/browser/ui/brave_browser_command_controller.h"
#include "brave/browser/ui/toolbar/brave_app_menu_model.h"
#include "brave/browser/ui/views/toolbar/brave_browser_app_menu_button.h"
#include "brave/components/brave_vpn/common/buildflags/buildflags.h"
#include "brave/components/skus/common/features.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(ENABLE_BRAVE_VPN)
#include "brave/browser/brave_vpn/brave_vpn_service_factory.h"
#include "brave/components/brave_vpn/browser/brave_vpn_service.h"
#include "brave/components/brave_vpn/common/features.h"
#endif

class BraveAppMenuBrowserTest : public InProcessBrowserTest {
 public:
  BraveAppMenuBrowserTest() {
#if BUILDFLAG(ENABLE_BRAVE_VPN)
    scoped_feature_list_.InitWithFeatures(
        {skus::features::kSkusFeature, brave_vpn::features::kBraveVPN}, {});
#endif
  }

  ~BraveAppMenuBrowserTest() override = default;

  BrowserAppMenuButton* menu_button() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->app_menu_button();
  }

#if BUILDFLAG(ENABLE_BRAVE_VPN)
  void SetPurchasedUserForBraveVPN(BrowserWindowInterface* browser,
                                   bool purchased) {
    auto* service =
        brave_vpn::BraveVpnServiceFactory::GetForProfile(browser->GetProfile());
    ASSERT_TRUE(!!service);
    auto target_state = purchased
                            ? brave_vpn::mojom::PurchasedState::PURCHASED
                            : brave_vpn::mojom::PurchasedState::NOT_PURCHASED;
    service->SetPurchasedStateForTesting(skus::GetDefaultEnvironment(),
                                         target_state);
    // Call explicitely to update vpn commands status because mojo works in
    // async way.
    static_cast<chrome::BraveBrowserCommandController*>(
        chrome::BrowserCommandController::From(browser))
        ->OnPurchasedStateChanged(target_state, std::nullopt);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

IN_PROC_BROWSER_TEST_F(BraveAppMenuBrowserTest, AppMenuButtonUpgradeAlertTest) {
  // Check property for our style.
  auto* brave_menu_button =
      static_cast<BraveBrowserAppMenuButton*>(menu_button());
  EXPECT_TRUE(brave_menu_button->ShouldPaintBorder());
  EXPECT_TRUE(brave_menu_button->ShouldBlendHighlightColor());

  // Check our highlight color.
  auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(brave_menu_button->GetHighlightColor(), std::nullopt);
  brave_menu_button->SetTypeAndSeverity(
      {AppMenuIconController::IconType::kUpgradeNotification,
       AppMenuIconController::Severity::kLow});
  EXPECT_EQ(brave_menu_button->GetHighlightColor(),
            SkColorSetRGB(0x00, 0x46, 0x07));
  brave_menu_button->SetTypeAndSeverity(
      {AppMenuIconController::IconType::kUpgradeNotification,
       AppMenuIconController::Severity::kMedium});
  EXPECT_EQ(brave_menu_button->GetHighlightColor(),
            SkColorSetRGB(0x4A, 0x39, 0x00));
  brave_menu_button->SetTypeAndSeverity(
      {AppMenuIconController::IconType::kUpgradeNotification,
       AppMenuIconController::Severity::kHigh});
  EXPECT_EQ(brave_menu_button->GetHighlightColor(),
            SkColorSetRGB(0x7D, 0x00, 0x1A));

  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kToolbarButtonHeight),
            brave_menu_button->height());
  EXPECT_EQ(::GetLayoutInsets(TOOLBAR_BUTTON), brave_menu_button->GetInsets());
}

#if BUILDFLAG(ENABLE_BRAVE_VPN)
// Check toggle menu item has additional toggle button for purchased user.
IN_PROC_BROWSER_TEST_F(BraveAppMenuBrowserTest, PurchasedVPN) {
  SetPurchasedUserForBraveVPN(browser(), true);
  menu_button()->ShowMenu();
  views::MenuItemView* menu_root = menu_button()->app_menu()->root_menu_item();
  auto* toggle_menu_item = menu_root->GetMenuItemByID(IDC_TOGGLE_BRAVE_VPN);
  ASSERT_TRUE(!!toggle_menu_item);
  const int last_item_index = toggle_menu_item->children().size() - 1;
  auto* toggle_button = views::AsViewClass<views::ToggleButton>(
      toggle_menu_item->children()[last_item_index]);
  ASSERT_NE(nullptr, toggle_button);
}

// Check app menu has show vpn panel menu item for not purchased user.
IN_PROC_BROWSER_TEST_F(BraveAppMenuBrowserTest, NotPurchasedVPN) {
  SetPurchasedUserForBraveVPN(browser(), false);
  menu_button()->ShowMenu();
  views::MenuItemView* menu_root = menu_button()->app_menu()->root_menu_item();
  EXPECT_TRUE(!!menu_root->GetMenuItemByID(IDC_SHOW_BRAVE_VPN_PANEL));
}
#endif

// Middle click on a "Recent tabs" entry should open the tab in the background
// and keep the menu open. MenuController only reaches
// AppMenu::ShouldExecuteCommandWithoutClosingMenu() for events that
// IsTriggerableEvent() accepts, and by default only left/right clicks are, so
// BraveAppMenu has to accept a middle click for recent tabs entries.
IN_PROC_BROWSER_TEST_F(BraveAppMenuBrowserTest,
                       RecentTabsMiddleClickIsTriggerable) {
  // Make sure the "Recently closed" section has a real entry to click on.
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  // CLOSE_CREATE_HISTORICAL_TAB makes TabRestoreService add a "Recently closed"
  // entry synchronously.
  tab_strip_model->CloseWebContentsAt(
      tab_strip_model->count() - 1, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  auto* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(tab_restore_service);
  ASSERT_FALSE(tab_restore_service->entries().empty());

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BraveAppMenuModel model(browser_view->toolbar(), browser());
  model.Init();
  const auto recent_tabs_index =
      model.GetIndexOfCommandId(AppMenuModel::kRecentTabsMenuPlaceholder);
  ASSERT_TRUE(recent_tabs_index);
  ui::MenuModel* recent_tabs_model =
      model.GetSubmenuModelAt(*recent_tabs_index);
  ASSERT_TRUE(recent_tabs_model);

  menu_button()->ShowMenu();
  AppMenu* menu = menu_button()->app_menu();
  ASSERT_TRUE(menu);
  views::MenuItemView* root = menu->root_menu_item();
  ASSERT_TRUE(root);

  const auto middle_click = [] {
    return ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), ui::EventTimeForNow(),
                          ui::EF_MIDDLE_MOUSE_BUTTON,
                          ui::EF_MIDDLE_MOUSE_BUTTON);
  };

  // "Recent tabs" entries use staggered command ids, see
  // AppMenuModel::kMinRecentTabsCommandId.
  constexpr int kFirstUnboundedCommandId = IDC_FIRST_UNBOUNDED_MENU;
  const int recent_tabs_command_offset =
      AppMenuModel::kMinRecentTabsCommandId - kFirstUnboundedCommandId;

  // A middle click must be triggerable for "Recent tabs" entries only - that is
  // what makes MenuController open them in a background tab and keep the menu
  // open - while the other items of the History submenu must ignore it.
  int recent_tabs_items = 0;
  for (size_t i = 0; i < recent_tabs_model->GetItemCount(); ++i) {
    const int command_id = recent_tabs_model->GetCommandIdAt(i);
    if (command_id < 0) {  // Separators and non-interactive titles.
      continue;
    }
    views::MenuItemView* item = root->GetMenuItemByID(command_id);
    if (!item) {
      continue;
    }
    const bool is_recent_tabs_entry =
        command_id >= kFirstUnboundedCommandId &&
        ((command_id - kFirstUnboundedCommandId) %
             AppMenuModel::kNumUnboundedMenuTypes ==
         recent_tabs_command_offset);
    EXPECT_EQ(is_recent_tabs_entry,
              menu->IsTriggerableEvent(item, middle_click()))
        << "command id: " << command_id;
    recent_tabs_items += is_recent_tabs_entry ? 1 : 0;
  }
  EXPECT_GT(recent_tabs_items, 0);

  // Middle clicking the History submenu item itself must keep doing nothing.
  views::MenuItemView* history_item =
      root->GetMenuItemByID(AppMenuModel::kRecentTabsMenuPlaceholder);
  ASSERT_TRUE(history_item);
  EXPECT_FALSE(menu->IsTriggerableEvent(history_item, middle_click()));
}
