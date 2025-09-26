#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include "SensorTreeModel.h"
#include <memory>

class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnGenerateTestData(wxCommandEvent& event);
    void CreateMenuBar();
    void SetupStatusBar();
    void CreateSensorTreeView();
    void PopulateTestData();

    enum
    {
        ID_Hello = 1,
        ID_GenerateTestData = 2
    };

    // UI components
    wxDataViewCtrl* m_treeCtrl;
    std::shared_ptr<SensorTreeModel> m_treeModel;

    // Event binding setup
    void BindEvents();
    void OnClose(wxCloseEvent& event);
};