#include "InGameMenu.h"

#include "ClientUI.h"
#include "CUIControls.h"
#include "OptionsWnd.h"
#include "SaveFileDialog.h"
#include "TextBrowseWnd.h"
#include "../client/human/GGHumanClientApp.h"
#include "../network/Networking.h"
#include "../util/i18n.h"
#include "../util/GameRules.h"
#include "../util/OptionsDB.h"
#include "../util/Logger.h"

#include <GG/Button.h>
#include <GG/Clr.h>
#include <GG/dialogs/ThreeButtonDlg.h>

#include <boost/filesystem/operations.hpp>

InGameMenu::InGameMenu():
    CUIWnd(UserString("GAME_MENU_WINDOW_TITLE"),
           GG::INTERACTIVE | GG::MODAL)
{}

void InGameMenu::CompleteConstruction() {
    CUIWnd::CompleteConstruction();

    m_save_btn = Wnd::Create<CUIButton>(UserString("GAME_MENU_SAVE"));
    if (GetApp().SinglePlayerGame())
        m_load_or_concede_btn = Wnd::Create<CUIButton>(UserString("GAME_MENU_LOAD"));
    else
        m_load_or_concede_btn = Wnd::Create<CUIButton>(UserString("GAME_MENU_CONCEDE"));
    m_options_btn = Wnd::Create<CUIButton>(UserString("INTRO_BTN_OPTIONS"));
    m_resign_btn = Wnd::Create<CUIButton>(UserString("GAME_MENU_RESIGN"));
    m_done_btn = Wnd::Create<CUIButton>(UserString("DONE"));

    AttachChild(m_save_btn);
    AttachChild(m_load_or_concede_btn);
    AttachChild(m_options_btn);
    AttachChild(m_resign_btn);
    AttachChild(m_done_btn);

    m_save_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Save, this));
    if (GetApp().SinglePlayerGame()) {
        m_load_or_concede_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Load, this));
    } else {
        m_load_or_concede_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Concede, this));
    }
    m_options_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Options, this));
    m_resign_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Resign, this));
    m_done_btn->LeftClickedSignal.connect(boost::bind(&InGameMenu::Done, this));

    if (!GetApp().CanSaveNow()) {
        m_save_btn->Disable();
        m_save_btn->SetBrowseModeTime(GetOptionsDB().Get<int>("ui.tooltip.delay"));
        m_save_btn->SetBrowseInfoWnd(GG::Wnd::Create<TextBrowseWnd>(
            UserString("BUTTON_DISABLED"),
            UserString("SAVE_DISABLED_BROWSE_TEXT"),
            GG::X(400)
        ));
    }

    if (!GetApp().SinglePlayerGame() &&
        !GetGameRules().Get<bool>("RULE_ALLOW_CONCEDE"))
    {
        m_load_or_concede_btn->Disable();
        m_load_or_concede_btn->SetBrowseModeTime(GetOptionsDB().Get<int>("ui.tooltip.delay"));
        m_load_or_concede_btn->SetBrowseInfoWnd(GG::Wnd::Create<TextBrowseWnd>(
            UserString("BUTTON_DISABLED"),
            UserString("ERROR_CONCEDE_DISABLED"),
            GG::X(400)
        ));
    }

    ResetDefaultPosition();
    DoLayout();
}

GG::Rect InGameMenu::CalculatePosition() const {
    static constexpr GG::X H_MAINMENU_MARGIN{40};  //horizontal empty space
    static constexpr GG::Y V_MAINMENU_MARGIN{40};  //vertical empty space

    // Calculate window width and height
    GG::Pt new_size(ButtonWidth() + H_MAINMENU_MARGIN,
                    GG::ToY(5.75 * ButtonCellHeight()) + V_MAINMENU_MARGIN); // 9 rows + 0.75 before exit button

    // This wnd determines its own position.
    GG::Pt new_ul((GetApp().AppWidth()  - new_size.x) / 2,
                  (GetApp().AppHeight() - new_size.y) / 2);

    return GG::Rect(new_ul, new_ul + new_size);
}

GG::X InGameMenu::ButtonWidth() const {
    static constexpr GG::X MIN_BUTTON_WIDTH{160};

    GG::X button_width(GG::X0); //width of the buttons

    button_width = std::max(button_width, m_save_btn->MinUsableSize().x);
    button_width = std::max(button_width, m_load_or_concede_btn->MinUsableSize().x);
    button_width = std::max(button_width, m_options_btn->MinUsableSize().x);
    button_width = std::max(button_width, m_resign_btn->MinUsableSize().x);
    button_width = std::max(button_width, m_done_btn->MinUsableSize().x);
    button_width = std::max(MIN_BUTTON_WIDTH, button_width);

    return button_width;
}

GG::Y InGameMenu::ButtonCellHeight() const {
    static constexpr GG::Y MIN_BUTTON_HEIGHT{40};
    return std::max(MIN_BUTTON_HEIGHT, m_done_btn->MinUsableSize().y);
}

void InGameMenu::DoLayout() {
    // place buttons
    GG::Pt button_ul(GG::X{15}, GG::Y{12});
    GG::Pt button_lr(ButtonWidth(), m_done_btn->MinUsableSize().y);

    button_lr += button_ul;

    const GG::Y button_cell_height = ButtonCellHeight();

    m_save_btn->SizeMove(button_ul, button_lr);
    button_ul.y += button_cell_height;
    button_lr.y += button_cell_height;
    m_load_or_concede_btn->SizeMove(button_ul, button_lr);
    button_ul.y += button_cell_height;
    button_lr.y += button_cell_height;
    m_resign_btn->SizeMove(button_ul, button_lr);
    button_ul.y += button_cell_height;
    button_lr.y += button_cell_height;
    m_options_btn->SizeMove(button_ul, button_lr);
    button_ul.y += button_cell_height * 7 / 8;
    button_lr.y += button_cell_height * 7 / 8;
    m_done_btn->SizeMove(button_ul, button_lr);
}

void InGameMenu::KeyPress(GG::Key key, uint32_t key_code_point,
                          GG::Flags<GG::ModKey> mod_keys)
{
    // Same behaviour as if "done" was pressed
    if (key == GG::Key::GGK_RETURN || key == GG::Key::GGK_KP_ENTER || key == GG::Key::GGK_ESCAPE )
        Done();
}

void InGameMenu::Save() {
    DebugLogger() << "InGameMenu::Save";

    auto& app = GetApp();

    if (!app.CanSaveNow()) {
        ErrorLogger() << "InGameMenu::Save aborting; Client app can't save now";
        return;
    }

    // send order changes could be made when player decide to save game
    app.SendPartialOrders();

    try {
        // When saving in multiplayer, you cannot see the old saves or
        // browse directories, only give a save file name.
        DebugLogger() << "... running save file dialog";
        auto filename = app.GetUI().GetFilenameWithSaveFileDialog(
            SaveFileDialog::Purpose::Save,
            app.SinglePlayerGame() ? SaveFileDialog::SaveType::SinglePlayer : SaveFileDialog::SaveType::MultiPlayer);

        if (!filename.empty()) {
            if (!app.CanSaveNow()) {
                ErrorLogger() << "InGameMenu::Save aborting; Client app can't save now";
                throw std::runtime_error(UserString("UNABLE_TO_SAVE_NOW_TRY_AGAIN"));
            }
            DebugLogger() << "... initiating save to " << filename ;
            app.SaveGame(filename);
            CloseClicked();
        }

    } catch (const std::exception& e) {
        ErrorLogger() << "Exception thrown attempting save: " << e.what();
        app.GetUI().MessageBox(e.what(), true);
    }
}

void InGameMenu::Load() {
    Hide();
    GetApp().LoadSinglePlayerGame();
    CloseClicked();
}

void InGameMenu::Options() {
    auto options_wnd = GG::Wnd::Create<OptionsWnd>(true);
    options_wnd->Run();
}

void InGameMenu::Concede() {
    auto& app = GetApp();

    // show confirmation dialog
    auto prompt = app.GetStyleFactory().NewThreeButtonDlg(
        GG::X(200), GG::Y(75), UserString("GAME_MENU_REALLY_CONCEDE"), app.GetUI().GetFont(),
        ClientUI::CtrlColor(), ClientUI::CtrlBorderColor(), ClientUI::CtrlColor(), ClientUI::TextColor(),
        2, UserString("YES"), UserString("CANCEL"));

    prompt->Run();

    if (prompt->Result() == 0) {
       // send ELIMINATE_SELF message
       app.EliminateSelf();
       CloseClicked();
    }
}

void InGameMenu::Resign() {
    // send order changes could be made when player decides to disconnect
    GetApp().SendPartialOrders();

    GetApp().ResetToIntro(false);
    CloseClicked();
}

void InGameMenu::Done()
{ m_modal_done.store(true); }
