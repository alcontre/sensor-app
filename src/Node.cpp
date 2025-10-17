#include "Node.h"

#include <algorithm>
#include <chrono>
#include <sstream>

Node::Node(const std::string &name, Node *parent) :
    m_name(name),
    m_parent(parent),
    m_hasValue(false),
    m_value(0.0), // Default constructor for DataValue
    m_lowerThreshold(std::nullopt),
    m_upperThreshold(std::nullopt),
    m_failed(false),
    m_lastUpdate(std::chrono::steady_clock::time_point{}),
    m_history(),
    m_historyLimit(1024),
    m_updateCount(0)
{
}

Node *Node::AddChild(std::unique_ptr<Node> child)
{
   if (!child)
      return nullptr;

   child->SetParent(this);
   Node *rawChild = child.get();
   // Insert in order received
   m_children.push_back(std::move(child));
   return rawChild;
}

Node *Node::FindChild(const std::string &name) const
{
   auto it = std::find_if(m_children.begin(), m_children.end(),
       [&name](const std::unique_ptr<Node> &child) {
          return child->GetName() == name;
       });

   return (it != m_children.end()) ? it->get() : nullptr;
}

void Node::SetValue(const DataValue &value,
    std::optional<DataValue> lowerThreshold,
    std::optional<DataValue> upperThreshold,
    bool failed)
{
   auto now         = std::chrono::steady_clock::now();
   m_value          = value;
   m_hasValue       = true;
   m_lowerThreshold = std::move(lowerThreshold);
   m_upperThreshold = std::move(upperThreshold);
   m_failed         = failed;
   m_lastUpdate     = now;
   ++m_updateCount;

   if (value.IsNumeric() && m_historyLimit > 0) {
      TimedSample sample{now, value.GetNumeric(), failed};
      m_history.push_back(sample);
      while (m_history.size() > m_historyLimit) {
         m_history.pop_front();
      }
   }
}

std::vector<std::string> Node::GetPath() const
{
   std::vector<std::string> path;
   const Node *current = this;

   while (current != nullptr) {
      path.insert(path.begin(), current->GetName());
      current = current->GetParent();
   }

   return path;
}

std::string Node::GetFullPath(const std::string &separator) const
{
   auto path = GetPath();
   if (path.empty())
      return "";

   std::ostringstream oss;
   for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0)
         oss << separator;
      oss << path[i];
   }
   return oss.str();
}

size_t Node::GetDepth() const
{
   size_t depth        = 0;
   const Node *current = m_parent;
   while (current != nullptr) {
      ++depth;
      current = current->GetParent();
   }
   return depth;
}

std::vector<Node *> Node::GetAllDescendants() const
{
   std::vector<Node *> descendants;
   GetAllDescendantsRecursive(descendants);
   return descendants;
}

std::vector<Node *> Node::GetLeafNodes() const
{
   std::vector<Node *> leaves;
   GetLeafNodesRecursive(leaves);
   return leaves;
}

void Node::GetAllDescendantsRecursive(std::vector<Node *> &nodes) const
{
   for (const auto &child : m_children) {
      nodes.push_back(child.get());
      child->GetAllDescendantsRecursive(nodes);
   }
}

void Node::GetLeafNodesRecursive(std::vector<Node *> &leaves) const
{
   if (IsLeaf()) {
      leaves.push_back(const_cast<Node *>(this));
   } else {
      for (const auto &child : m_children) {
         child->GetLeafNodesRecursive(leaves);
      }
   }
}

double Node::GetSecondsSinceUpdate() const
{
   if (!m_hasValue)
      return 0.0;

   auto now     = std::chrono::steady_clock::now();
   auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_lastUpdate);
   return elapsed.count();
}

void Node::SetHistoryLimit(size_t limit)
{
   if (limit == 0)
      limit = 1;
   m_historyLimit = limit;
   while (m_history.size() > m_historyLimit) {
      m_history.pop_front();
   }
}

void Node::ClearHistory()
{
   m_history.clear();
}