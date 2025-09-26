#include "MainFrame.h"
#include "SensorTreeModel.h"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Sensor Tree Viewer", 
              wxDefaultPosition, wxSize(800, 600))
    , m_treeCtrl(nullptr)
{
    CreateMenuBar();
    SetupStatusBar();
    CreateSensorTreeView();
    BindEvents();
    
    // Populate initial test data
    // PopulateTestData();
    
    SetStatusText("Welcome to Sensor Tree Viewer! Sample data loaded.");
}

void MainFrame::CreateMenuBar()
{
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&About...\tCtrl-A",
                     "Show information about this application");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu* menuData = new wxMenu;
    menuData->Append(ID_GenerateTestData, "&Generate Test Data\tCtrl-G",
                     "Generate sample hierarchical sensor data");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuData, "&Data");

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

void MainFrame::OnGenerateTestData(wxCommandEvent& event)
{
    PopulateTestData();
    SetStatusText("Test data generated!");
}

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

void MainFrame::PopulateTestData()
{
    if (!m_treeModel)
    {
        SetStatusText("Error: Tree model not initialized!");
        return;
    }
    
    SetStatusText("Populating test data...");
    
    // Create a simple test by building the tree structure manually
    // and then setting the model
    
    // Create root nodes
    auto server01 = std::make_shared<Node>("Server01");
    auto server02 = std::make_shared<Node>("Server02");
    auto network = std::make_shared<Node>("Network");
    
    // Build Server01 structure
    auto cpu01 = std::make_shared<Node>("CPU");
    server01->AddChild(cpu01);
    
    auto core0 = std::make_shared<Node>("Core0");
    auto core1 = std::make_shared<Node>("Core1");
    cpu01->AddChild(core0);
    cpu01->AddChild(core1);
    
    auto temp_core0 = std::make_shared<Node>("Temperature");
    temp_core0->SetValue(DataValue(45.2));
    core0->AddChild(temp_core0);
    
    auto volt_core0 = std::make_shared<Node>("Voltage");
    volt_core0->SetValue(DataValue(1.2));
    core0->AddChild(volt_core0);
    
    auto temp_core1 = std::make_shared<Node>("Temperature");
    temp_core1->SetValue(DataValue(42.8));
    core1->AddChild(temp_core1);
    
    // Build Server02 structure
    auto status02 = std::make_shared<Node>("Status");
    status02->SetValue(DataValue("Online"));
    server02->AddChild(status02);
    
    // Build Network structure
    auto router01 = std::make_shared<Node>("Router01");
    network->AddChild(router01);
    
    auto port1 = std::make_shared<Node>("Port1");
    router01->AddChild(port1);
    
    auto linkStatus = std::make_shared<Node>("LinkStatus");
    linkStatus->SetValue(DataValue("Up"));
    port1->AddChild(linkStatus);
    
    // Clear and set the new structure
    m_treeModel->ClearAll();
    m_treeModel->AddRootNode(server01);
    m_treeModel->AddRootNode(server02);
    m_treeModel->AddRootNode(network);
    
    SetStatusText("Test data populated successfully!");
}

void MainFrame::BindEvents()
{
    // Bind menu events using modern Bind() syntax
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, ID_Hello);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnGenerateTestData, this, ID_GenerateTestData);
    // Bind close event to ensure model is disassociated before destruction
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    // Ensure the data view control disassociates the model before it is destroyed.
    if (m_treeCtrl && m_treeModel)
    {
        // Disassociate model to prevent wxDataViewCtrl from trying to remove notifier
        m_treeCtrl->AssociateModel(nullptr);
    }

    // Proceed with default close handling
    event.Skip();
}