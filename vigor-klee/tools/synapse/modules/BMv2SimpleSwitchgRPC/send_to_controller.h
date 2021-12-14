#pragma once

#include "../../log.h"
#include "../module.h"
#include "call-paths-to-bdd.h"

namespace synapse {
namespace targets {
namespace BMv2SimpleSwitchgRPC {

class SendToController : public Module {
private:
  uint64_t metadata_code_path;

public:
  SendToController()
      : Module(ModuleType::BMv2SimpleSwitchgRPC_SendToController,
               Target::BMv2SimpleSwitchgRPC, "SendToController") {
    next_target = Target::x86;
  }

  SendToController(BDD::BDDNode_ptr node, uint64_t _metadata_code_path)
      : Module(ModuleType::BMv2SimpleSwitchgRPC_SendToController,
               Target::BMv2SimpleSwitchgRPC, "SendToController", node),
        metadata_code_path(_metadata_code_path) {
    next_target = Target::x86;
  }

private:
  void replace_next(BDD::BDDNode_ptr prev, BDD::BDDNode_ptr old_next,
                    BDD::BDDNode_ptr new_next) const {
    assert(prev);
    assert(old_next);
    assert(new_next);

    if (prev->get_type() == BDD::Node::NodeType::BRANCH) {
      auto branch_node = static_cast<BDD::Branch *>(prev.get());

      if (branch_node->get_on_true()->get_id() == old_next->get_id()) {
        branch_node->replace_on_true(new_next);
      } else {
        branch_node->replace_on_false(new_next);
      }

      new_next->replace_prev(prev);
      return;
    }

    prev->replace_next(new_next);
    new_next->replace_prev(prev);
  }

  BDD::BDDNode_ptr clone_calls(ExecutionPlan &ep,
                               BDD::BDDNode_ptr current) const {
    assert(current);

    if (!current->get_prev()) {
      return current;
    }

    auto node = current;
    auto root = current;
    auto next = current->get_next();
    auto prev = current->get_prev();

    auto &bdd = ep.get_bdd();

    while (node->get_prev()) {
      node = node->get_prev();

      if (node->get_type() == BDD::Node::NodeType::CALL) {
        auto clone = node->clone();

        clone->replace_next(root);
        clone->replace_prev(nullptr);

        root->replace_prev(clone);

        clone->update_id(bdd.get_and_inc_id());

        root = clone;
      }
    }

    replace_next(prev, current, root);

    return root;
  }

  processing_result_t process(const ExecutionPlan &ep, BDD::BDDNode_ptr node) {
    processing_result_t result;

    auto ep_cloned = ep.clone(true);
    auto &bdd = ep_cloned.get_bdd();
    auto node_cloned = bdd.get_node_by_id(node->get_id());

    auto next = clone_calls(ep_cloned, node_cloned);
    auto _metadata_code_path = node->get_id();

    auto new_module =
        std::make_shared<SendToController>(node_cloned, _metadata_code_path);
    auto next_ep = ep_cloned.add_leaves(new_module, next, false, false);
    next_ep.replace_active_leaf_node(next, false);

    result.module = new_module;
    result.next_eps.push_back(next_ep);

    return result;
  }

  processing_result_t process_branch(const ExecutionPlan &ep,
                                     BDD::BDDNode_ptr node,
                                     const BDD::Branch *casted) override {
    return process(ep, node);
  }

  processing_result_t process_call(const ExecutionPlan &ep,
                                   BDD::BDDNode_ptr node,
                                   const BDD::Call *casted) override {
    return process(ep, node);
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor) const override {
    visitor.visit(this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new SendToController(node, metadata_code_path);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SendToController *>(other);

    if (metadata_code_path != other_cast->metadata_code_path) {
      return false;
    }

    return true;
  }

  uint64_t get_metadata_code_path() const { return metadata_code_path; }
};
} // namespace BMv2SimpleSwitchgRPC
} // namespace targets
} // namespace synapse
