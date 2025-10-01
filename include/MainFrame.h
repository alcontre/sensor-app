#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/timer.h>
#include <wx/event.h>
#include "SensorTreeModel.h"
#include <memory>
#include <vector>

// Custom event carrying sensor data samples queued into the UI thread
wxDECLARE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

class SensorDataEvent : public wxCommandEvent
{
public:
    SensorDataEvent();
    SensorDataEvent(const std::vector<std::string>& path, const DataValue& value);
    SensorDataEvent(const SensorDataEvent& other);

    // Required for wxWidgets event cloning
    virtual wxEvent* Clone() const override;

    const std::vector<std::string>& GetPath() const { return m_path; }
    const DataValue& GetValue() const { return m_value; }

private:
    std::vector<std::string> m_path;
    DataValue m_value;
};

class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    
    void CreateMenuBar();
    void SetupStatusBar();
    void CreateSensorTreeView();

    enum
    {
        ID_Hello = 1,
        ID_DataTimer,
        ID_AgeTimer,

        // Menu bar
        ID_ExpandAll,
        ID_CollapseAll,

        // Context menu entries
        ID_ExpandAllHere,
        ID_CollapseChildrenHere
    };

    // UI components
    wxDataViewCtrl* m_treeCtrl;
    std::shared_ptr<SensorTreeModel> m_treeModel;
    wxTimer m_dataTimer;
    wxTimer m_ageTimer;
    bool m_generationActive;
    uint64_t m_samplesReceived;
    // Track the item for which a context menu is opened
    wxDataViewItem m_contextItem;

    // Event binding setup
    void BindEvents();
    void OnClose(wxCloseEvent& event);
    void OnDataTimer(wxTimerEvent& event);
    void OnAgeTimer(wxTimerEvent& event);
    void OnSensorData(wxCommandEvent& event);
    void OnExpandAll(wxCommandEvent& event);
    void OnItemActivated(wxDataViewEvent& event);
    void OnItemContextMenu(wxDataViewEvent& event);
    void OnExpandAllHere(wxCommandEvent& event);
    void OnCollapseChildrenHere(wxCommandEvent& event);
    void OnCollapseAll(wxCommandEvent& event);
    void StartDataGeneration();
    void StopDataGeneration();
    void QueueRandomDataSample();
};