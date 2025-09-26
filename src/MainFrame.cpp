#include "MainFrame.h"

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_Hello, MainFrame::OnAbout)
    EVT_MENU(wxID_EXIT, MainFrame::OnExit)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Sensor Tree Viewer", 
              wxDefaultPosition, wxSize(800, 600))
{
    CreateMenuBar();
    SetupStatusBar();
    
    // Create main panel
    wxPanel* panel = new wxPanel(this, wxID_ANY);
    
    // Create a simple sizer for now
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Add a placeholder text for now
    wxStaticText* text = new wxStaticText(panel, wxID_ANY, 
        "Sensor Tree Viewer - Ready for data model implementation");
    sizer->Add(text, 0, wxALL | wxCENTER, 20);
    
    panel->SetSizer(sizer);
    
    SetStatusText("Welcome to Sensor Tree Viewer!");
}

void MainFrame::CreateMenuBar()
{
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&About...\tCtrl-A",
                     "Show information about this application");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");

    SetMenuBar(menuBar);
}

void MainFrame::SetupStatusBar()
{
    CreateStatusBar();
    SetStatusText("Ready");
}

void MainFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("Sensor Tree Viewer\n"
                 "A hierarchical sensor data display application",
                 "About Sensor Tree Viewer", wxOK | wxICON_INFORMATION);
}