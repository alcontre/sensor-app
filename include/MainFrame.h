#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>

class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void CreateMenuBar();
    void SetupStatusBar();

    enum
    {
        ID_Hello = 1
    };

    wxDECLARE_EVENT_TABLE();
};