#pragma once
#include "SensorData.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
class Node;

// Generic hierarchical node that can represent any level in the tree
class Node
{
 public:
   Node(const std::string &name, Node *parent = nullptr);
   ~Node() = default;

   // Basic properties
   const std::string &GetName() const { return m_name; }
   void SetName(const std::string &name) { m_name = name; }

   // Hierarchy management
   Node *GetParent() const { return m_parent; }
   void SetParent(Node *parent) { m_parent = parent; }

   const std::vector<std::unique_ptr<Node>> &GetChildren() const { return m_children; }
   Node *AddChild(std::unique_ptr<Node> child);
   Node *FindChild(const std::string &name) const;

   // Data value (leaf nodes can have values)
   bool HasValue() const { return m_hasValue; }
   const DataValue &GetValue() const { return m_value; }
   const std::optional<DataValue> &GetLowerThreshold() const { return m_lowerThreshold; }
   const std::optional<DataValue> &GetUpperThreshold() const { return m_upperThreshold; }
   bool IsFailed() const { return m_failed; }
   void SetValue(const DataValue &value,
       std::optional<DataValue> lowerThreshold = std::nullopt,
       std::optional<DataValue> upperThreshold = std::nullopt,
       bool failed                             = false);
   void ClearValue();
   double GetSecondsSinceUpdate() const;

   // Tree utilities
   std::vector<std::string> GetPath() const;
   std::string GetFullPath(const std::string &separator = "/") const;
   size_t GetDepth() const;
   bool IsLeaf() const { return m_children.empty(); }
   bool IsRoot() const { return m_parent == nullptr; }

   // Tree traversal
   std::vector<Node *> GetAllDescendants() const;
   std::vector<Node *> GetLeafNodes() const;

 private:
   std::string m_name;
   Node *m_parent;
   std::vector<std::unique_ptr<Node>> m_children;

   bool m_hasValue;
   DataValue m_value;
   std::optional<DataValue> m_lowerThreshold;
   std::optional<DataValue> m_upperThreshold;
   bool m_failed;
   std::chrono::steady_clock::time_point m_lastUpdate;

   void GetAllDescendantsRecursive(std::vector<Node *> &nodes) const;
   void GetLeafNodesRecursive(std::vector<Node *> &leaves) const;
};