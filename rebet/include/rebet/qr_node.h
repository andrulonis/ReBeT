#pragma once

#include "behaviortree_cpp/decorator_node.h"
#include "behaviortree_cpp/behavior_tree.h"
#include "rclcpp/rclcpp.hpp"
#include "builtin_interfaces/msg/time.hpp"
#include <algorithm>
#include <chrono>
#include <ctime> 
#include "rebet/system_attribute_value.hpp"

namespace BT
{
/**
 * @brief The QRNode is used to specify quality requirements which may influence the behavior of the nodes it decorates..
 */

enum class QualityAttribute {Power, Safety, TaskEfficiency, MovementEfficiency, Test};

class QRNode : public DecoratorNode
{
public:

  QRNode(const std::string& name, const NodeConfig& config, QualityAttribute quality_attribute) : DecoratorNode(name, config)
  {
    _quality_attribute = quality_attribute;
    _average_metric = 0.0;
    _times_calculated = 0;
  }

  static PortsList providedPorts()
  {
      return {InputPort<double>(WEIGHT, "How much influence this QR should have in the calculation of system utility"), 
              OutputPort<double>(METRIC, "To what extent is this property fulfilled"),
              OutputPort<double>(MEAN_METRIC, "To what extent is this property fulfilled on average"),
              OutputPort<std::string>(QR_STATUS, "Information as to the state the QR is currently in."),
              };
  }

  virtual ~QRNode() override = default;

  //Name for the weight input port
  static constexpr const char* WEIGHT = "weight";

  //Names for the metric output ports
  static constexpr const char* METRIC = "metric";
  static constexpr const char* MEAN_METRIC = "mean_metric";
  static constexpr const char* QR_STATUS = "out_status";

  QualityAttribute qa_type()
  {
    return _quality_attribute;
  }

  double current_metric()
  {
    return _metric;
  }

  bool is_higher_better()
  {
    return _higher_is_better;
  }

private:
  int weight_;
  bool read_parameter_from_ports_;
  virtual NodeStatus tick() override
  {
    setStatus(NodeStatus::RUNNING);
    // calculate measure before running child
    calculate_measure();
    const NodeStatus child_status = child_node_->executeTick();

    switch (child_status)
    {
      case NodeStatus::SUCCESS: {
        calculate_measure(); // calculate one last time
        resetChild();
        return NodeStatus::SUCCESS;
      }

      case NodeStatus::FAILURE: {
        resetChild();
        return NodeStatus::FAILURE;
      }

      case NodeStatus::RUNNING: {
        return NodeStatus::RUNNING;
      }

      case NodeStatus::SKIPPED: {
        return NodeStatus::SKIPPED;
      }
      case NodeStatus::IDLE: {
        throw LogicError("[", name(), "]: A child should not return IDLE");
      }
    }
    return status();

  }

  protected:
    QualityAttribute _quality_attribute;
    int _times_calculated;
    double _average_metric;
    double _metric;
    bool _higher_is_better = true;


    void metric_mean()
    {
      double new_average = _average_metric + ((_metric - _average_metric) / (double)_times_calculated);
      _average_metric = new_average;
    }

    void output_metric()
    {
      setOutput(METRIC,_metric);
      _times_calculated++; 
    }

    virtual void calculate_measure()
    {
      getInput(WEIGHT, weight_);
      std::stringstream ss;

      ss << "Weight Port info received: ";
      ss << weight_;
      std::cout << ss.str().c_str() << std::endl;
      std::cout << "Here's where I would calculate a measure... if I had one" << std::endl;
    }
};

class TaskLevelQR : public QRNode
{
  public:
    TaskLevelQR(const std::string& name, const NodeConfig& config, QualityAttribute quality_attribute) : QRNode(name, config, quality_attribute)
    {
    }

};

class SystemLevelQR : public QRNode
{
  public:
    SystemLevelQR(const std::string& name, const NodeConfig& config, QualityAttribute quality_attribute) : QRNode(name, config, quality_attribute)
    {
    }
 
  protected:
    void gather_child_metrics()
      {

        auto node_visitor = [this](TreeNode* node)
        {
          if (auto task_qr_node = dynamic_cast<TaskLevelQR*>(node))
          {
            if(_quality_attribute == task_qr_node->qa_type()) //If child TaskQR QA matches SystemLevelQR's QA.
            {
              double task_qr_metric = task_qr_node->current_metric();
              std::string task_qr_name = task_qr_node->registrationName();
              _sub_qr_metrics[task_qr_name] = task_qr_metric;
            }
          }
        };

        BT::applyRecursiveVisitor(child_node_, node_visitor);
      }

    std::map<std::string, double> _sub_qr_metrics;

    virtual NodeStatus tick() override
    {
      setStatus(NodeStatus::RUNNING);
      const NodeStatus child_status = child_node_->executeTick();

      switch (child_status)
      {
        case NodeStatus::SUCCESS: {
          resetChild();
          return NodeStatus::SUCCESS;
        }

        case NodeStatus::FAILURE: {
          resetChild();
          return NodeStatus::FAILURE;
        }

        case NodeStatus::RUNNING: {
          calculate_measure();
          return NodeStatus::RUNNING;
        }

        case NodeStatus::SKIPPED: {
          return NodeStatus::SKIPPED;
        }
        case NodeStatus::IDLE: {
          throw LogicError("[", name(), "]: A child should not return IDLE");
        }
      }
      return status();

    }
};

}   // namespace BT