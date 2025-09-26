#include "MainFrame.h"
#include "SensorTreeModel.h"
#include <random>
#include <chrono>

wxDEFINE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

SensorDataEvent::SensorDataEvent()
    : wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE)
    , m_value(0.0)
{
}

SensorDataEvent::SensorDataEvent(const std::vector<std::string>& path, const DataValue& value)
    : wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE)
    , m_path(path)
    , m_value(value)
{
}

SensorDataEvent::SensorDataEvent(const SensorDataEvent& other)
    : wxCommandEvent(other)
    , m_path(other.m_path)
    , m_value(other.m_value)
{
}

wxEvent* SensorDataEvent::Clone() const
{
    return new SensorDataEvent(*this);
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Sensor Tree Viewer", 
              wxDefaultPosition, wxSize(800, 600))
    , m_treeCtrl(nullptr)
    , m_dataTimer(this, ID_DataTimer)
    , m_generationActive(false)
    , m_samplesReceived(0)
{
    CreateMenuBar();
    SetupStatusBar();
    CreateSensorTreeView();
    BindEvents();
    
    // Start automatic data generation (will run indefinitely)
    StartDataGeneration();

    SetStatusText("Welcome to Sensor Tree Viewer! Auto data generation started.");
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

    // View menu: expand/collapse helpers
    wxMenu* menuView = new wxMenu;
    menuView->Append(ID_ExpandAll, "&Expand All\tCtrl-E", "Expand all nodes in the tree view");
    menuBar->Append(menuView, "&View");

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
                 "A hierarchical sensor data display application\n"
                 "Supports arbitrary hierarchical data structures",
                 "About Sensor Tree Viewer", wxOK | wxICON_INFORMATION);
}

// Removed manual test data generation; data is created by the auto-generator

// Start/stop handlers removed; generation starts automatically

void MainFrame::CreateSensorTreeView()
{
    // Create main panel
    wxPanel* panel = new wxPanel(this, wxID_ANY);
    
    // Create the tree model
    m_treeModel = std::make_shared<SensorTreeModel>();
    
    // Create the data view control
    m_treeCtrl = new wxDataViewCtrl(panel, wxID_ANY, 
                                   wxDefaultPosition, wxDefaultSize,
                                   wxDV_MULTIPLE | wxDV_ROW_LINES | wxDV_HORIZ_RULES);
    
    // Associate the model with the control
    m_treeCtrl->AssociateModel(m_treeModel.get());
    
    // Add columns
    m_treeCtrl->AppendTextColumn("Name", SensorTreeModel::COL_NAME, wxDATAVIEW_CELL_INERT, 200);
    m_treeCtrl->AppendTextColumn("Value", SensorTreeModel::COL_VALUE, wxDATAVIEW_CELL_INERT, 120);
    m_treeCtrl->AppendTextColumn("Type", SensorTreeModel::COL_TYPE, wxDATAVIEW_CELL_INERT, 100);
    
    // Layout
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_treeCtrl, 1, wxEXPAND | wxALL, 5);
    panel->SetSizer(sizer);
}

// PopulateTestData removed: tree will be populated dynamically by incoming samples

void MainFrame::BindEvents()
{
    // Bind menu events using modern Bind() syntax
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, ID_Hello);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    // Bind close event to ensure model is disassociated before destruction
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_TIMER, &MainFrame::OnDataTimer, this, ID_DataTimer);
    Bind(wxEVT_SENSOR_DATA_SAMPLE, &MainFrame::OnSensorData, this);
    Bind(wxEVT_MENU, &MainFrame::OnExpandAll, this, ID_ExpandAll);
}

void MainFrame::OnDataTimer(wxTimerEvent& event)
{
    if (!m_generationActive)
        return;

    QueueRandomDataSample();
}

void MainFrame::OnSensorData(wxCommandEvent& event)
{
    auto* sampleEvent = dynamic_cast<SensorDataEvent*>(&event);
    if (!sampleEvent || !m_treeModel)
        return;

    m_treeModel->AddDataSample(sampleEvent->GetPath(), sampleEvent->GetValue());
    ++m_samplesReceived;
    SetStatusText(wxString::Format("Samples received: %zu", (unsigned long long)m_samplesReceived));
}

// Helper to recursively expand items starting from a parent item
static void ExpandAllRecursive(wxDataViewCtrl* ctrl, const wxDataViewItem& parent, const SensorTreeModel* model)
{
    wxDataViewItemArray children;
    model->GetChildren(parent, children);
    for (const wxDataViewItem& child : children)
    {
        // Expand this child
        ctrl->Expand(child);
        // Recurse
        ExpandAllRecursive(ctrl, child, model);
    }
}

void MainFrame::OnExpandAll(wxCommandEvent& event)
{
    if (!m_treeCtrl || !m_treeModel)
        return;

    // Start from the invisible root
    wxDataViewItem root = wxDataViewItem(NULL);
    ExpandAllRecursive(m_treeCtrl, root, m_treeModel.get());
}

void MainFrame::StartDataGeneration()
{
    if (m_generationActive)
        return;

    m_generationActive = true;
    if (!m_dataTimer.IsRunning())
    {
        // Generate a first sample immediately
        QueueRandomDataSample();
        m_dataTimer.Start(1000);
    }
    SetStatusText("Auto data generation started");
}

void MainFrame::StopDataGeneration()
{
    if (!m_generationActive)
        return;

    m_generationActive = false;
    if (m_dataTimer.IsRunning())
    {
        m_dataTimer.Stop();
    }
    SetStatusText("Auto data generation stopped");
}

void MainFrame::QueueRandomDataSample()
{
    struct SampleDefinition
    {
        std::vector<std::string> path;
        enum class ValueType { Numeric, String } type;
        double minValue;
        double maxValue;
        std::vector<std::string> stringOptions;
    };

    static const std::vector<SampleDefinition> definitions = {
        { {"Server01", "CPU", "Core0", "Temperature"}, SampleDefinition::ValueType::Numeric, 35.0, 65.0, {} },
        { {"Server01", "CPU", "Core0", "Voltage"}, SampleDefinition::ValueType::Numeric, 1.0, 1.2, {} },
        { {"Server01", "CPU", "Core1", "Temperature"}, SampleDefinition::ValueType::Numeric, 35.0, 65.0, {} },
        { {"Server01", "GPU", "Temperature"}, SampleDefinition::ValueType::Numeric, 45.0, 80.0, {} },
        { {"Server01", "GPU", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Running", "Idle", "Throttled"} },
        { {"Server02", "CPU", "Temperature"}, SampleDefinition::ValueType::Numeric, 32.0, 60.0, {} },
        { {"Server02", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Online", "Maintenance", "Offline"} },
        { {"Network", "Router01", "Port1", "Throughput"}, SampleDefinition::ValueType::Numeric, 1000.0, 10000.0, {} },
        { {"Network", "Router01", "Port1", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Up", "Down", "Flapping"} },
        { {"Network", "Router01", "Port2", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Up", "Down"} }
    };

    if (definitions.empty())
        return;

    static std::mt19937 rng(static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    std::uniform_int_distribution<size_t> defDist(0, definitions.size() - 1);
    const auto& def = definitions[defDist(rng)];

    DataValue value(0.0);
    if (def.type == SampleDefinition::ValueType::Numeric)
    {
        std::uniform_real_distribution<double> valDist(def.minValue, def.maxValue);
        value = DataValue(valDist(rng));
    }
    else
    {
        if (def.stringOptions.empty())
            return;
        std::uniform_int_distribution<size_t> strDist(0, def.stringOptions.size() - 1);
        value = DataValue(def.stringOptions[strDist(rng)]);
    }

    auto* evt = new SensorDataEvent(def.path, value);
    wxQueueEvent(this, evt);
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    StopDataGeneration();
    // Ensure the data view control disassociates the model before it is destroyed.
    if (m_treeCtrl && m_treeModel)
    {
        // Disassociate model to prevent wxDataViewCtrl from trying to remove notifier
        m_treeCtrl->AssociateModel(nullptr);
    }

    // Proceed with default close handling
    event.Skip();
}