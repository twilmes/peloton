//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// index_scan_plan.h
//
// Identification: src/include/planner/index_scan_plan.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "planner/abstract_scan_plan.h"
#include "common/types.h"
#include "expression/abstract_expression.h"
#include "storage/tuple.h"
#include "index/scan_optimizer.h"

namespace peloton {

namespace index {
class Index;
}

namespace storage {
class Tuple;
}

namespace planner {

class IndexScanPlan : public AbstractScan {
 public:
  /*
   * class IndexScanDesc - Stores information to do the index scan
   */
  struct IndexScanDesc {

    /*
     * Default Constructor - Set index pointer to empty
     *
     * We need to do this since this might be created even when an index
     * is not required, e.g. inside hybrid scan
     */
    IndexScanDesc() : index_obj{nullptr} {}

    /*
     * Constructor
     *
     * This constructor is called when an index scan is required. For the
     * situations where index is not required, the default constructor should
     * be called to notify later procedures of the absense of an index
     */
    IndexScanDesc(
        std::shared_ptr<index::Index> p_index_obj,
        const std::vector<oid_t> &p_tuple_column_id_list,
        const std::vector<ExpressionType> &expr_list_p,
        const std::vector<common::Value *> &p_value_list,
        const std::vector<expression::AbstractExpression *> &p_runtime_key_list)
        : index_obj(p_index_obj),
          tuple_column_id_list(p_tuple_column_id_list),
          expr_list(expr_list_p),
          value_list(p_value_list),
          runtime_key_list(p_runtime_key_list) {}

    ~IndexScanDesc() {
      //for (auto val : value_list)
      //  delete val;
    }

    // The index object for scanning
    //
    // NOTE: For hybrid scan plans, even if there is no index required
    // for a scan, an empty scan descriptor will still be passed in as
    // argument. This is a bad design but currently we have to live with it
    // In order to prevent the scan predicate optimizer from trying to
    // optimizing the index scan while the index pointer is not valid
    // this should be set to 0 for an empty initialization
    std::shared_ptr<index::Index> index_obj;

    // A list of columns id in the base table that has a scan predicate
    // (only for indexed column in the base table)
    std::vector<oid_t> tuple_column_id_list;

    // A list of expressions
    std::vector<ExpressionType> expr_list;

    // A list of values either bounded or unbounded
    std::vector<common::Value *> value_list;

    // ???
    std::vector<expression::AbstractExpression *> runtime_key_list;
  };

  ///////////////////////////////////////////////////////////////////
  // Members of IndexScanPlan
  ///////////////////////////////////////////////////////////////////

  IndexScanPlan(const IndexScanPlan &) = delete;
  IndexScanPlan &operator=(const IndexScanPlan &) = delete;
  IndexScanPlan(IndexScanPlan &&) = delete;
  IndexScanPlan &operator=(IndexScanPlan &&) = delete;

  IndexScanPlan(storage::DataTable *table,
                expression::AbstractExpression *predicate,
                const std::vector<oid_t> &column_ids,
                const IndexScanDesc &index_scan_desc, bool for_update_flag = false);

  ~IndexScanPlan() {
    for (auto expr : runtime_keys_) {
      delete expr;
    }
    for (auto val : values_) {
      delete val;
    }
    for (auto val : values_with_params_) {
      delete val;
    }
    LOG_TRACE("Destroyed a index scan plan!");
  }

  std::shared_ptr<index::Index> GetIndex() const { return index_; }

  const std::vector<oid_t> &GetColumnIds() const { return column_ids_; }

  const std::vector<oid_t> &GetKeyColumnIds() const { return key_column_ids_; }

  const std::vector<ExpressionType> &GetExprTypes() const {
    return expr_types_;
  }

  const index::IndexScanPredicate &GetIndexPredicate() const {
    return index_predicate_;
  }

  const std::vector<common::Value *> &GetValues() const { return values_; }

  const std::vector<expression::AbstractExpression *> &GetRunTimeKeys() const {
    return runtime_keys_;
  }

  inline PlanNodeType GetPlanNodeType() const {
    return PLAN_NODE_TYPE_INDEXSCAN;
  }

  const std::string GetInfo() const { return "IndexScan"; }

  void SetParameterValues(std::vector<common::Value *> *values);

  std::unique_ptr<AbstractPlan> Copy() const {
    std::vector<expression::AbstractExpression *> new_runtime_keys;
    for (auto *key : runtime_keys_) {
      new_runtime_keys.push_back(key->Copy());
    }

    IndexScanDesc desc(index_, key_column_ids_, expr_types_, values_,
                       new_runtime_keys);
    IndexScanPlan *new_plan = new IndexScanPlan(
        GetTable(), GetPredicate()->Copy(), GetColumnIds(), desc , false);
    return std::unique_ptr<AbstractPlan>(new_plan);
  }

 private:
  /** @brief index associated with index scan. */
  std::shared_ptr<index::Index> index_;

  // A list of column IDs involved in the index scan no matter whether
  // it is indexed or not (i.e. select statement)
  const std::vector<oid_t> column_ids_;

  // A list of column IDs involved in the index scan that are indexed by
  // the index choen inside the optimizer
  const std::vector<oid_t> key_column_ids_;

  const std::vector<ExpressionType> expr_types_;

  // LM: I removed a const keyword for binding purpose
  //
  // The life time of the scan predicate is as long as the lifetime
  // of this array, since we always use the values in this array to
  // construct low key and high key for the scan plan, it should stay
  // valid until the scan plan is destructed
  //
  // Note that when binding values to the scan plan we copy those values
  // into this array, which means the lifetime of values being bound is
  // also the lifetime of the IndexScanPlan object
  std::vector<common::Value *> values_;
  // the original copy of values with all the value parameters (bind them later)
  std::vector<common::Value *> values_with_params_;

  const std::vector<expression::AbstractExpression *> runtime_keys_;

  // Currently we just support single conjunction predicate
  //
  // In the future this might be extended into an array of conjunctive
  // predicates connected by disjunction
  index::IndexScanPredicate index_predicate_;
};

}  // namespace planner
}  // namespace peloton
