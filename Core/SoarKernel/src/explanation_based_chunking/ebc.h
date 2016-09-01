/*
 * ebc.h
 *
 *  Created on: Dec 15, 2015
 *      Author: Mazin Assanie
 */

#ifndef EBC_MANAGER_H_
#define EBC_MANAGER_H_

#include "kernel.h"
#include "stl_typedefs.h"

#include <list>
#include <set>
#include <unordered_map>
#include <cstdlib>

#define BUFFER_PROD_NAME_SIZE 256
#define CHUNK_COND_HASH_TABLE_SIZE 1024
#define LOG_2_CHUNK_COND_HASH_TABLE_SIZE 10

#define BUILD_WITH_EXPLAINER

tc_number get_new_tc_number(agent* thisAgent);

typedef struct constraint_struct
{
    test eq_test;
    test constraint_test;
    constraint_struct(test new_eq, test new_constraint) : eq_test(new_eq), constraint_test(new_constraint) {}
} constraint;

typedef struct attachment_struct
{
        condition* cond;
        WME_Field field;
        attachment_struct(condition* new_cond, WME_Field new_field) : cond(new_cond), field(new_field) {}

} attachment_point;

typedef struct chunk_cond_struct
{
    condition* cond;                /* points to the original condition */

    condition* instantiated_cond;   /* points to cond in chunk instantiation */
    condition* variablized_cond;    /* points to cond in the actual chunk */

    /* dll of all cond's in a set (i.e., a chunk_cond_set, or the grounds) */
    struct chunk_cond_struct* next, *prev;

    /* dll of cond's in this particular hash bucket for this set */
    struct chunk_cond_struct* next_in_bucket, *prev_in_bucket;

    uint32_t hash_value;             /* equals hash_condition(cond) */
    uint32_t compressed_hash_value;  /* above, compressed to a few bits */
} chunk_cond;

typedef struct chunk_cond_set_struct
{
    chunk_cond* all;       /* header for dll of all chunk_cond's in the set */
    chunk_cond* table[CHUNK_COND_HASH_TABLE_SIZE];  /* hash table buckets */
} chunk_cond_set;

typedef struct backtrace_struct
{
    int result;                    /* 1 when this is a result of the chunk */
    condition* trace_cond;         /* The (local) condition being traced */
    char   prod_name[BUFFER_PROD_NAME_SIZE];         /* The production's name */
    condition* grounds;            /* The list of conds for the LHS of chunk */
    condition* potentials;         /* The list of conds which aren't linked */
    condition* locals;             /* Conds in the subgoal -- need to BT */
    condition* negated;            /* Negated conditions (sub/super) */
    struct backtrace_struct* next_backtrace; /* Pointer to next in this list */
} backtrace_str;

class Explanation_Based_Chunker
{
        friend class Repair_Manager;

    public:

        Explanation_Based_Chunker(agent* myAgent);
        ~Explanation_Based_Chunker();

        /* Settings and cli command related functions */
        ebc_param_container*    ebc_params;
        bool                    ebc_settings[num_ebc_settings];
        uint64_t                max_chunks, max_dupes;
        bool                    max_chunks_reached;

        /* --- lists of symbols (PS names) declared chunk-free and chunky --- */
        ::list*     chunk_free_problem_spaces;
        ::list*     chunky_problem_spaces;   /* AGR MVL1 */

        /* Builds a chunk or justification based on a submitted instantiation
         * and adds it to the rete.  Called by create_instantiation, smem and epmem */
        void build_chunk_or_justification(instantiation* inst, instantiation** custom_inst_list);

        /* Methods used during instantiation creation to generate identities used by the
         * explanation trace. */
        void add_explanation_to_condition(rete_node* node, condition* cond,
                                          node_varnames* nvn, uint64_t pI_id,
                                          AddAdditionalTestsMode additional_tests);
        uint64_t get_new_inst_id() { increment_counter(inst_id_counter); return inst_id_counter; };
        uint64_t get_instantiation_count() { return inst_id_counter; };
        uint64_t get_justification_count() { return justification_count; };
        uint64_t get_or_create_o_id(Symbol* orig_var, uint64_t pI_id);
        Symbol * get_ovar_for_o_id(uint64_t o_id);
        Symbol*  get_match_for_rhs_var(Symbol* pRHS_var);

        /* Methods used during condition copying to make unification and constraint
         * attachment more effecient */
        void unify_identity(test t);
        bool in_null_identity_set(test t);
        tc_number get_constraint_found_tc_num() { return tc_num_found; };

        /* Determines whether learning is on for a particular instantiation
         * based on the global learning settings and whether the state chunky */
        bool set_learning_for_instantiation(instantiation* inst);
        void set_failure_type(EBCFailureType pFailure_type) {m_failure_type = pFailure_type; };
        void reset_chunks_this_d_cycle() { chunks_this_d_cycle = 0; justifications_this_d_cycle = 0;};

        /* RL templates utilize the EBChunker variablization code when building
         * template instances.  We make these two methods public to support that. */
        void        variablize_condition_list   (condition* top_cond, bool pInNegativeCondition = false);
        action*     variablize_rl_action        (action* pRLAction, struct token_struct* tok, wme* w, double & initial_value);

        /* Methods for printing in Soar trace */
        void print_current_built_rule(const char* pHeader = NULL);

        /* Debug printing methods */
        void print_variablization_table(TraceMode mode);
        void print_tables(TraceMode mode);
        void print_o_id_tables(TraceMode mode);
        void print_attachment_points(TraceMode mode);
        void print_constraints(TraceMode mode);
        void print_merge_map(TraceMode mode);
        void print_ovar_to_o_id_map(TraceMode mode);
        void print_o_id_substitution_map(TraceMode mode);
        void print_o_id_to_ovar_debug_map(TraceMode mode);

        void print_chunking_summary();
        void print_chunking_settings();

        /* Clean-up */
        void reinit();
        void cleanup_after_instantiation_creation(uint64_t pI_id);
        void cleanup_for_instantiation_deallocation(uint64_t pI_id);
        void clear_variablization_maps();

    private:

        agent*              thisAgent;
        Output_Manager*     outputManager;

        /* Statistics on learning performed so far */
        uint64_t            chunk_count;
        uint64_t            justification_count;
        uint64_t            chunks_this_d_cycle;
        uint64_t            justifications_this_d_cycle;
        std::string*        chunk_history;

        /* String that every chunk name begins with */
        char*               chunk_name_prefix;
        char*               justification_name_prefix;

        /* -- A counter for variablization and instantiation id's - */
        uint64_t inst_id_counter;
        uint64_t ovar_id_counter;

        tc_number tc_num_found;

        /* Variables used by dependency analysis methods */
        ::list*             grounds;
        ::list*             locals;
        ::list*             positive_potentials;
        chunk_cond_set      negated_set;
        tc_number           grounds_tc;
        tc_number           locals_tc;
        tc_number           potentials_tc;
        tc_number           backtrace_number;
        bool                quiescence_t_flag;

        /* Variables used by result building methods */
        bool                m_learning_on_for_instantiation;
        instantiation*      m_inst;
        preference*         m_results;
        goal_stack_level    m_results_match_goal_level;
        uint64_t            m_chunk_new_i_id;
        tc_number           m_results_tc;
        preference*         m_extra_results;
        bool                m_reliable;
        condition*          m_inst_top;
        condition*          m_inst_bottom;
        condition*          m_vrblz_top;
        action*             m_rhs;
        production*         m_prod;
        instantiation*      m_chunk_inst;
        Symbol*             m_prod_name;
        condition*          m_saved_justification_top;
        condition*          m_saved_justification_bottom;
        ProductionType      m_prod_type;
        bool                m_should_print_name, m_should_print_prod;
        EBCFailureType      m_failure_type;

        /* -- The following are the core tables used by EBC during
         *    instantiation creation, identity analysis, condition
         *    formation and variablization.  The data stored within
         *    them is temporary and cleared after use. -- */

        inst_to_id_map*            instantiation_identities;
        id_to_sym_map*             o_id_to_var_map;
        id_to_sym_map*             id_to_rule_sym_debug_map;

        id_to_id_map*              unification_map;
        identity_triple*                local_singleton_superstate_identity;

        constraint_list*                constraints;
        attachment_points_map*     attachment_points;

        /* -- Table of previously seen conditions.  Used to determine whether to
         *    merge or eliminate positive conditions on the LHS of a chunk. -- */
        triple_merge_map*               cond_merge_map;

        /* Used by repair manager if it needs to find original matched value for
         * variablized rhs item. */
        sym_to_sym_map*            rhs_var_to_match_map;

        bool learning_is_on_for_instantiation() { return m_learning_on_for_instantiation; };

        /* Explanation/identity generation methods */
        void            add_identity_to_id_test(condition* cond, byte field_num, rete_node_level levels_up);
        void            add_constraint_to_explanation(test* dest_test_address, test new_test, uint64_t pI_id, bool has_referent = true);
        void            add_explanation_to_RL_condition(rete_node* node, condition* cond, node_varnames* nvn,
                                                        uint64_t pI_id, AddAdditionalTestsMode additional_tests);
        /* Chunk building methods */
        Symbol*         generate_chunk_name(instantiation* inst, bool pIsChunk);
        void            set_up_rule_name(bool pForChunk);
        bool            can_learn_from_instantiation();
        void            get_results_for_instantiation();
        void            add_goal_or_impasse_tests();
        void            add_pref_to_results(preference* pref, uint64_t linked_id);
        void            add_results_for_id(Symbol* id, uint64_t linked_id);
        void            add_results_if_needed(Symbol* sym, uint64_t linked_id);
        action*         copy_action_list(action* actions);
        void            init_chunk_cond_set(chunk_cond_set* set);
        void            create_initial_chunk_condition_lists();
        bool            add_to_chunk_cond_set(chunk_cond_set* set, chunk_cond* new_cc);
        chunk_cond*     make_chunk_cond_for_negated_condition(condition* cond);
        void            make_clones_of_results();
        void            create_instantiated_counterparts();
        void            remove_from_chunk_cond_set(chunk_cond_set* set, chunk_cond* cc);
        void            reorder_instantiated_conditions(condition* top_cond, condition** dest_inst_top, condition** dest_inst_bottom);
        bool            reorder_and_validate_chunk();
        void            deallocate_failed_chunk();
        void            clean_up();
        void            save_conditions_for_reversal();
        void            revert_chunk_to_instantiation();
        void            add_chunk_to_rete();

        /* Dependency analysis methods */
        void perform_dependency_analysis();
        void add_to_grounds(condition* cond);
        void add_to_potentials(condition* cond);
        void add_to_locals(condition* cond);
        void trace_locals(goal_stack_level grounds_level);
        void trace_grounded_potentials();
        bool trace_ungrounded_potentials(goal_stack_level grounds_level);
        void backtrace_through_instantiation(
                instantiation* inst,
                goal_stack_level grounds_level,
                condition* trace_cond,
                const identity_triple o_ids_to_replace,
                const rhs_triple rhs_funcs,
                uint64_t bt_depth,
                BTSourceType bt_type);
        void report_local_negation(condition* c);

        /* Identity analysis and unification methods */
        uint64_t get_existing_o_id(Symbol* orig_var, uint64_t pI_id);
        void add_identity_unification(uint64_t pOld_o_id, uint64_t pNew_o_id);
        void update_unification_table(uint64_t pOld_o_id, uint64_t pNew_o_id, uint64_t pOld_o_id_2 = 0);
        void create_consistent_identity_for_result_element(preference* result, uint64_t pNew_i_id, WME_Field field);
        void unify_backtraced_conditions(condition* parent_cond, const identity_triple o_ids_to_replace, const rhs_triple rhs_funcs);
        void add_singleton_unification_if_needed(condition* pCond);
        void add_local_singleton_unification_if_needed(condition* pCond);
        void literalize_RHS_function_args(const rhs_value rv);
        void merge_conditions(condition* top_cond);

        /* Constraint analysis and enforcement methods */
        void cache_constraints_in_cond(condition* c);
        void add_additional_constraints();
        bool has_positive_condition(uint64_t pO_id);
        void cache_constraints_in_test(test t);
        void reset_constraint_found_tc_num() { if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return; tc_num_found = get_new_tc_number(thisAgent); };
        attachment_point* get_attachment_point(uint64_t pO_id);
        void set_attachment_point(uint64_t pO_id, condition* pCond, WME_Field pField);
        void find_attachment_points(condition* cond);
        void prune_redundant_constraints();
        void invert_relational_test(test* pEq_test, test* pRelational_test);
        void attach_relational_test(test pEq_test, test pRelational_test);

        /* Variablization methods */
        action* variablize_result_into_actions(preference* result, bool variablize);
        action* variablize_results_into_actions(preference* result, bool variablize);
        void variablize_lhs_symbol(Symbol** sym, uint64_t pIdentity);
        void variablize_rhs_symbol(rhs_value pRhs_val, bool pShouldCachedMatchValue = false);
        void variablize_equality_tests(test t);
        bool variablize_test_by_lookup(test t, bool pSkipTopLevelEqualities);
        void variablize_tests_by_lookup(test t, bool pSkipTopLevelEqualities);
        void store_variablization(Symbol* instantiated_sym, Symbol* variable, uint64_t pIdentity);
        Symbol* get_variablization(uint64_t index_id);
        void add_matched_sym_for_rhs_var(Symbol* pRHS_var, Symbol* pMatched_sym);

        /* Condition polishing methods */
        void        remove_ungrounded_sti_from_test_and_cache_eq_test(test* t);
        void        merge_values_in_conds(condition* pDestCond, condition* pSrcCond);
        condition*  get_previously_seen_cond(condition* pCond);

        /* Clean-up methods */
        void clear_merge_map();
        void clear_o_id_to_ovar_debug_map();
        void clear_rulesym_to_identity_map();
        void clear_attachment_map();
        void clear_cached_constraints();
        void clear_o_id_substitution_map();
        void clear_singletons();
        void clear_data();
        void clear_rhs_var_to_match_map() { rhs_var_to_match_map->clear(); };

};

#endif /* EBC_MANAGER_H_ */