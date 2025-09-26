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
    void OnGenerateTestData(wxCommandEvent& event);
    void CreateMenuBar();
    void SetupStatusBar();
    void CreateSensorTreeView();
    void PopulateTestData();

    enum
    {
        ID_Hello = 1,
        ID_GenerateTestData = 2,
        ID_StartGenerator,
        ID_StopGenerator,
        ID_DataTimer
    };

    // UI components
    wxDataViewCtrl* m_treeCtrl;
    std::shared_ptr<SensorTreeModel> m_treeModel;
    wxTimer m_dataTimer;
    bool m_generationActive;

    // Event binding setup
    void BindEvents();
    void OnClose(wxCloseEvent& event);
    void OnDataTimer(wxTimerEvent& event);
    void OnSensorData(wxCommandEvent& event);
    void OnStartGenerator(wxCommandEvent& event);
    void OnStopGenerator(wxCommandEvent& event);
    void StartDataGeneration();
    void StopDataGeneration();
    void QueueRandomDataSample();
};