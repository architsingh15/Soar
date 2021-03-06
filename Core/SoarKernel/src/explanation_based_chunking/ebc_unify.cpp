/*
 * variablization_manager_map.cpp
 *
 *  Created on: Jul 25, 2013
 *      Author: mazzin
 */

#include "ebc.h"
#include "ebc_timers.h"

#include "agent.h"
#include "condition.h"
#include "dprint.h"
#include "explanation_memory.h"
#include "instantiation.h"
#include "output_manager.h"
#include "preference.h"
#include "print.h"
#include "rhs.h"
#include "rhs_functions.h"
#include "symbol.h"
#include "symbol_manager.h"
#include "test.h"
#include "working_memory.h"

#include <assert.h>

bool Explanation_Based_Chunker::in_null_identity_set(test t)
{
    std::unordered_map< uint64_t, uint64_t >::iterator iter = (*unification_map).find(t->identity);
    if (iter != (*unification_map).end()) return (iter->second == NULL_IDENTITY_SET);
    return (t->identity == NULL_IDENTITY_SET);
}

uint64_t Explanation_Based_Chunker::get_identity(uint64_t pID)
{
    if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return NULL_IDENTITY_SET;
    std::unordered_map< uint64_t, uint64_t >::iterator iter = (*unification_map).find(pID);
    if (iter != (*unification_map).end()) return iter->second;
    return pID;
}

/* Not used any more */
void Explanation_Based_Chunker::unify_preference_identities(preference* lPref)
{
    if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return;

    rhs_value lRHS;
    if (lPref->identities.id) lPref->identities.id = get_identity(lPref->identities.id);
    if (lPref->identities.attr) lPref->identities.attr = get_identity(lPref->identities.attr);
    if (lPref->identities.value) lPref->identities.value = get_identity(lPref->identities.value);
    if (lPref->identities.referent) lPref->identities.referent = get_identity(lPref->identities.referent);
    if (lPref->rhs_funcs.id)
    {
        lRHS = copy_rhs_value(thisAgent, lPref->rhs_funcs.id, true);
        deallocate_rhs_value(thisAgent, lPref->rhs_funcs.id);
        lPref->rhs_funcs.id = lRHS;
    }
    if (lPref->rhs_funcs.attr)
    {
        lRHS = copy_rhs_value(thisAgent, lPref->rhs_funcs.attr, true);
        deallocate_rhs_value(thisAgent, lPref->rhs_funcs.attr);
        lPref->rhs_funcs.attr = lRHS;
    }
    if (lPref->rhs_funcs.value)
    {
        lRHS = copy_rhs_value(thisAgent, lPref->rhs_funcs.value, true);
        deallocate_rhs_value(thisAgent, lPref->rhs_funcs.value);
        lPref->rhs_funcs.value = lRHS;
    }
    if (lPref->rhs_funcs.referent)
    {
        lRHS = copy_rhs_value(thisAgent, lPref->rhs_funcs.referent, true);
        deallocate_rhs_value(thisAgent, lPref->rhs_funcs.referent);
        lPref->rhs_funcs.referent = lRHS;
    }

}
void Explanation_Based_Chunker::update_unification_table(uint64_t pOld_o_id, uint64_t pNew_o_id, uint64_t pOld_o_id_2)
{
    ebc_timers->identity_unification->stop();
    ebc_timers->identity_update->start();
    std::unordered_map< uint64_t, uint64_t >::iterator iter;
    assert(ebc_settings[SETTING_EBC_LEARNING_ON]);

    for (iter = unification_map->begin(); iter != unification_map->end(); ++iter)
    {

        if ((iter->second == pOld_o_id) || (pOld_o_id_2 && (iter->second == pOld_o_id_2)))
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "...found secondary o_id unification mapping that needs updated: %u = %u -> %u = %u.\n", iter->first, iter->second, iter->first, pNew_o_id);
            (*unification_map)[iter->first] = pNew_o_id;
            thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_transitive, iter->first, pNew_o_id);
        }
    }
    ebc_timers->identity_update->stop();
    ebc_timers->identity_unification->start();
}

void Explanation_Based_Chunker::add_identity_unification(uint64_t pOld_o_id, uint64_t pNew_o_id)
{
    assert(ebc_settings[SETTING_EBC_LEARNING_ON]);

    if (pOld_o_id == NULL_IDENTITY_SET) return;
    ebc_timers->variablization_rhs->start();
    ebc_timers->variablization_rhs->stop();
    ebc_timers->identity_unification->start();

    if (pOld_o_id == pNew_o_id)
    {
        dprint(DT_ADD_IDENTITY_SET_MAPPING, "Attempting to unify identical conditions for identity %u].  Skipping.\n", pNew_o_id);
        ebc_timers->identity_unification->stop();
        return;
    }
    dprint(DT_ADD_IDENTITY_SET_MAPPING, "Adding identity unification %u -> %u\n", pOld_o_id, pNew_o_id);

    std::unordered_map< uint64_t, uint64_t >::iterator iter;
    uint64_t newID;

    if (pNew_o_id == 0)
    {
        /* Do not check if a unification already exists if we're propagating back a literalization */
        dprint(DT_ADD_IDENTITY_SET_MAPPING, "Adding literalization substitution: %u -> 0.\n", pOld_o_id);
        newID = 0;
    } else {
        /* See if a unification already exists for the new identity propagating back*/
        iter = (*unification_map).find(pNew_o_id);

        if (iter == (*unification_map).end())
        {
            /* Map all cases of this identity with its parent identity */
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Did not find existing mapping for %u.  Adding %u -> %u.\n", pNew_o_id, pOld_o_id, pNew_o_id);
            newID = pNew_o_id;
        }
        else if (iter->second == pOld_o_id)
        {
            /* Circular reference */
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "o_id unification (%u -> %u) already exists.  Transitive mapping %u -> %u would be self referential.  Not adding.\n",
                pNew_o_id, iter->second, pOld_o_id, iter->second);
            ebc_timers->identity_unification->stop();
            return;
        }
        else
        {
            /* Map all cases of what this identity is already remapped to with its parent identity */
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "o_id unification (%u -> %u) already exists.  Adding transitive mapping %u -> %u.\n",
                pNew_o_id, iter->second, pOld_o_id, iter->second);
            newID = iter->second;
        }
    }

    /* See if a unification already exists for the identity being replaced in this instantiation*/
    iter = (*unification_map).find(pOld_o_id);
    uint64_t existing_mapping;
    if (iter != (*unification_map).end())
    {
        existing_mapping = iter->second;
        if (existing_mapping == 0)
        {
            if (newID != 0)
            {
                /* The existing identity we're unifying with is already literalized from a different trace.  So,
                 * literalize any tests with identity of parent in this trace */
                dprint(DT_ADD_IDENTITY_SET_MAPPING, "Literalization exists for %u.  Propagating literalization substitution with %u -> 0.\n", pOld_o_id, pNew_o_id);
                (*unification_map)[newID] = 0;
                thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_unified_with_literalized_identity, newID, 0);
                update_unification_table(newID, 0);
            } else {
                dprint(DT_ADD_IDENTITY_SET_MAPPING, "Literalizing %u which is already literalized.  Skipping %u -> 0.\n", pOld_o_id, pNew_o_id);
            }
        } else {
            if (newID == existing_mapping)
            {
                dprint(DT_ADD_IDENTITY_SET_MAPPING, "The unification %u -> %u already exists.  Skipping.\n", pOld_o_id, newID);
            }
            else if (newID == 0)
            {
                /* The existing identity we're literalizing is already unified with another identity from
                 * a different trace.  So, literalize the identity, that it is already remapped to.*/
                dprint(DT_ADD_IDENTITY_SET_MAPPING, "Unification with another identity exists for %u.  Propagating literalization substitution with %u -> 0.\n", pOld_o_id, existing_mapping);
                (*unification_map)[existing_mapping] = 0;
                thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_literalize_mappings_exist, existing_mapping, 0);
                update_unification_table(existing_mapping, 0, pOld_o_id);
            } else {
                /* The existing identity we're unifying with is already unified with another identity from
                 * a different trace.  So, unify the identity that it is already remapped to with identity
                 * of the parent in this trace */
                dprint(DT_ADD_IDENTITY_SET_MAPPING, "Unification with another identity exists for %u.  Adding %u -> %u.\n", pOld_o_id, existing_mapping, pNew_o_id);
                (*unification_map)[newID] = existing_mapping;
                thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_unified_with_existing_mappings, newID, existing_mapping);
                update_unification_table(newID, existing_mapping);
                if (pNew_o_id != newID)
                {
                    (*unification_map)[pNew_o_id] = existing_mapping;
                    thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_unified_with_existing_mappings, pNew_o_id, existing_mapping);
                    update_unification_table(pNew_o_id, existing_mapping);
                }
            }
        }
    } else {
        (*unification_map)[pOld_o_id] = newID;
        thisAgent->explanationMemory->add_identity_set_mapping(m_current_bt_inst_id, IDS_no_existing_mapping, pOld_o_id, newID);
        update_unification_table(pOld_o_id, newID);
    }
    ebc_timers->identity_unification->stop();

    /* Unify identity in this instantiation with final identity */
//    dprint(DT_ADD_IDENTITY_SET_MAPPING, "New identity propagation map:\n");
//    dprint_unification_map(DT_ADD_IDENTITY_SET_MAPPING);
}

void Explanation_Based_Chunker::literalize_RHS_function_args(const rhs_value rv, uint64_t inst_id)
{
    /* Assign identities of all arguments in rhs fun call to null identity set*/
    cons* fl = rhs_value_to_funcall_list(rv);
    rhs_function_struct* rf = static_cast<rhs_function_struct*>(fl->first);
    cons* c;

    assert(ebc_settings[SETTING_EBC_LEARNING_ON]);

    if (rf->can_be_rhs_value)
    {
        for (c = fl->rest; c != NIL; c = c->rest)
        {
            if (rhs_value_is_funcall(static_cast<char*>(c->first)))
            {
                if (rhs_value_is_literalizing_function(static_cast<char*>(c->first)))
                {
                    dprint(DT_RHS_FUN_VARIABLIZATION, "Recursive call to literalize RHS function argument %r\n", static_cast<char*>(c->first));
                    literalize_RHS_function_args(static_cast<char*>(c->first), inst_id);
                }
            } else {
                dprint(DT_RHS_FUN_VARIABLIZATION, "Literalizing RHS function argument %r ", static_cast<char*>(c->first));
                assert(rhs_value_is_symbol(static_cast<char*>(c->first)));
                rhs_symbol rs = rhs_value_to_rhs_symbol(static_cast<char*>(c->first));
                dprint_noprefix(DT_RHS_FUN_VARIABLIZATION, "[%y %u]\n", rs->referent, rs->o_id);
                if (rs->o_id && !rs->referent->is_sti())
                {
                    thisAgent->explanationMemory->add_identity_set_mapping(inst_id, IDS_literalized_RHS_function_arg, rs->o_id, 0);
                    add_identity_unification(rs->o_id, 0);
                    thisAgent->explanationMemory->increment_stat_rhs_arguments_literalized(m_rule_type);
                }
            }
        }
    }
}

void Explanation_Based_Chunker::unify_backtraced_conditions(condition* lhs_cond,
                                                         const identity_quadruple rhs_id_sets,
                                                         const rhs_quadruple rhs_funcs)
{
    test lId = 0, lAttr = 0, lValue = 0;
    lId = lhs_cond->data.tests.id_test->eq_test;
    lAttr = lhs_cond->data.tests.attr_test->eq_test;
    lValue = lhs_cond->data.tests.value_test->eq_test;

    assert(ebc_settings[SETTING_EBC_LEARNING_ON]);

    dprint(DT_ADD_IDENTITY_SET_MAPPING, "Unifying backtraced condition.  Parent cond = %l, identities to replace = (%u ^%u %u)  [referent %u]\n", lhs_cond, rhs_id_sets.id, rhs_id_sets.attr, rhs_id_sets.value, rhs_id_sets.referent);

    if (rhs_id_sets.id)
    {
        if (lId->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a shared identity for identifier element: %u -> %u\n", rhs_id_sets.id, lId->identity);
        } else {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found an identity to literalize for identifier element: %u -> %t\n", rhs_id_sets.id, lId);
        }
        add_identity_unification(rhs_id_sets.id, lId->identity);
    }
    else
    {
        if (rhs_funcs.id && rhs_value_is_literalizing_function(rhs_funcs.id))
        {
            literalize_RHS_function_args(rhs_funcs.id, lhs_cond->inst->i_id);
            add_identity_unification(lId->identity, NULL_IDENTITY_SET);
        }
        else if (lId->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a condition testing a literal.  Will literalize identifier element %u in test %t\n", lId->identity, lId);
            add_identity_unification(lId->identity, NULL_IDENTITY_SET);
        }

    }
    if (rhs_id_sets.attr)
    {
        if (lAttr->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a shared identity for attribute element: %u -> %u\n", rhs_id_sets.attr, lAttr->identity);
        } else {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found an identity to literalize for attribute element: %u -> %t\n", rhs_id_sets.attr, lAttr);
        }
        add_identity_unification(rhs_id_sets.attr, lAttr->identity);
    }
    else
    {
        if (rhs_value_is_literalizing_function(rhs_funcs.attr))
        {
            literalize_RHS_function_args(rhs_funcs.attr, lhs_cond->inst->i_id);
            add_identity_unification(lAttr->identity, NULL_IDENTITY_SET);
        }
        else if (lAttr->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a condition testing a literal.  Will literalize attribute element %u in test %t\n", lAttr->identity, lAttr);
            add_identity_unification(lAttr->identity, NULL_IDENTITY_SET);
        }
    }
    if (rhs_id_sets.value)
    {
        if (lValue->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a shared identity to replace for value element: %u -> %u\n", rhs_id_sets.value, lValue->identity);
        } else {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found an identity to literalize for value element: %u -> %t\n", rhs_id_sets.value, lValue);
        }
        add_identity_unification(rhs_id_sets.value, lValue->identity);
    }
    else
    {
        if (rhs_value_is_literalizing_function(rhs_funcs.value))
        {
            literalize_RHS_function_args(rhs_funcs.value, lhs_cond->inst->i_id);
            add_identity_unification(lValue->identity, NULL_IDENTITY_SET);
        }
        else if (lValue->identity)
        {
            dprint(DT_ADD_IDENTITY_SET_MAPPING, "Found a condition testing a literal.  Will literalize value element %u in test %t\n", lValue->identity, lValue);
            add_identity_unification(lValue->identity, NULL_IDENTITY_SET);
        }
    }
    assert(!rhs_id_sets.referent);
    if (rhs_value_is_literalizing_function(rhs_funcs.referent))
    {
        dprint(DT_ADD_IDENTITY_SET_MAPPING, "Literalizing referent in RHS function %r\n", rhs_funcs.referent);
        literalize_RHS_function_args(rhs_funcs.referent, lhs_cond->inst->i_id);
    }
}

/* Requires: pCond is a local condition */
void Explanation_Based_Chunker::add_local_singleton_unification_if_needed(condition* pCond)
{
    if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return;

    if ((pCond->bt.wme_->attr == thisAgent->symbolManager->soarSymbols.superstate_symbol) &&
        (pCond->bt.wme_->id->id->isa_goal) && (pCond->bt.wme_->value->is_sti() && pCond->bt.wme_->value->id->isa_goal))
    {
        ebc_timers->dependency_analysis->stop();
        if (local_singleton_superstate_identity.id == 0)
        {
//            dprint(DT_UNIFY_SINGLETONS, "Storing identities for local singleton wme: %l\n", pCond);
            local_singleton_superstate_identity = { pCond->data.tests.id_test->eq_test->identity, pCond->data.tests.attr_test->eq_test->identity, pCond->data.tests.value_test->eq_test->identity };
        } else {
//            dprint(DT_UNIFY_SINGLETONS, "Unifying local singleton wme: %l\n", pCond);
            if (pCond->data.tests.value_test->eq_test->identity || local_singleton_superstate_identity.value)
            {
//                dprint(DT_UNIFY_SINGLETONS, "...unifying value element %u -> %u\n", pCond->data.tests.value_test->eq_test->identity, local_singleton_superstate_identity.value);
                thisAgent->explanationMemory->add_identity_set_mapping(pCond->inst->i_id, IDS_unified_with_singleton, pCond->data.tests.value_test->eq_test->identity, local_singleton_superstate_identity.value);
                add_identity_unification(pCond->data.tests.value_test->eq_test->identity, local_singleton_superstate_identity.value);
            }
        }
        ebc_timers->dependency_analysis->start();
    }

}

/* Requires: pCond is being added to grounds and is the second condition being added to grounds
 *           that matched a given wme, which guarantees chunker_bt_last_ground_cond points to the
 *           first condition that matched. */
void Explanation_Based_Chunker::add_singleton_unification_if_needed(condition* pCond)
{
//    if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return; // May need this if we have to add both conds when learning is off
    assert(ebc_settings[SETTING_EBC_LEARNING_ON]);

    if (wme_is_a_singleton(pCond->bt.wme_) || ebc_settings[SETTING_EBC_UNIFY_ALL])
    {
        condition* last_cond = pCond->bt.wme_->chunker_bt_last_ground_cond;
        assert(last_cond);
        dprint(DT_UNIFY_SINGLETONS, "Unifying value element of second condition that matched singleton wme: %l\n", pCond);
        dprint(DT_UNIFY_SINGLETONS, "-- Original condition seen: %l\n", pCond->bt.wme_->chunker_bt_last_ground_cond);
        if (pCond->data.tests.value_test->eq_test->identity || last_cond->data.tests.value_test->eq_test->identity)
        {
            ebc_timers->dependency_analysis->stop();
            ebc_timers->dependency_analysis->start();
            if (!pCond->data.tests.value_test->eq_test->identity)
            {
                thisAgent->explanationMemory->add_identity_set_mapping(pCond->inst->i_id, IDS_unified_with_singleton, pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
                add_identity_unification(last_cond->data.tests.value_test->eq_test->identity, NULL_IDENTITY_SET);
            } else if (!last_cond->data.tests.value_test->eq_test->identity)
            {
                thisAgent->explanationMemory->add_identity_set_mapping(pCond->inst->i_id, IDS_unified_with_singleton, pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
                add_identity_unification(pCond->data.tests.value_test->eq_test->identity, NULL_IDENTITY_SET);
            } else
            {
                thisAgent->explanationMemory->add_identity_set_mapping(pCond->inst->i_id, IDS_unified_with_singleton, pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
                add_identity_unification(pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
            }
        }
    }
    /* The code that sets isa_operator checks if an id is a goal, so don't need to check here */
    else if ((pCond->bt.wme_->attr == thisAgent->symbolManager->soarSymbols.operator_symbol) &&
        (pCond->bt.wme_->value->is_sti() &&  pCond->bt.wme_->value->id->isa_operator) &&
        (!pCond->test_for_acceptable_preference))
    {
        condition* last_cond = pCond->bt.wme_->chunker_bt_last_ground_cond;
        assert(last_cond);
        if (pCond->data.tests.value_test->eq_test->identity || last_cond->data.tests.value_test->eq_test->identity)
        {
            ebc_timers->dependency_analysis->stop();
            thisAgent->explanationMemory->add_identity_set_mapping(pCond->inst->i_id, IDS_unified_with_singleton, pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
            add_identity_unification(pCond->data.tests.value_test->eq_test->identity, last_cond->data.tests.value_test->eq_test->identity);
            ebc_timers->dependency_analysis->start();
        }
    }
}

const std::string Explanation_Based_Chunker::add_new_singleton(singleton_element_type id_type, Symbol* attrSym, singleton_element_type value_type)
{
    std::string returnVal;

    if ((attrSym == thisAgent->symbolManager->soarSymbols.operator_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.superstate_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.smem_sym) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.type_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.impasse_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.epmem_sym))
    {
        thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Soar cannot override the architectural singleton for %y.  Ignoring.", attrSym);
        return returnVal;
    }

    if (attrSym->sc->singleton.possible)
    {
        thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Clearing previous singleton for %y.\n", attrSym);
    }
    thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Will unify conditions in super-states that match a WME that fits the pattern:  (%s ^%y %s)", singletonTypeToString(id_type), attrSym, singletonTypeToString(value_type));
    singletons->insert(attrSym);
    thisAgent->symbolManager->symbol_add_ref(attrSym);
    attrSym->sc->singleton.possible = true;
    attrSym->sc->singleton.id_type = id_type;
    attrSym->sc->singleton.value_type = value_type;

    return returnVal;
}

const std::string Explanation_Based_Chunker::remove_singleton(singleton_element_type id_type, Symbol* attrSym, singleton_element_type value_type)
{
    std::string returnVal;

    if ((attrSym == thisAgent->symbolManager->soarSymbols.operator_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.superstate_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.smem_sym) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.type_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.impasse_symbol) ||
        (attrSym == thisAgent->symbolManager->soarSymbols.epmem_sym))
    {
        thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Soar cannot remove the architectural singleton for %y.  Ignoring.", attrSym);
        return returnVal;
    }
    auto it = singletons->find(attrSym);
    if (it == singletons->end())
    {
        thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Could not find pattern (%s ^%y %s).  Did not remove.", singletonTypeToString(id_type), attrSym, singletonTypeToString(value_type));
    } else {
        thisAgent->outputManager->sprinta_sf(thisAgent, returnVal, "Removed. Will no longer unify conditions in super-states that match a WME\n"
                                                                   "         that fits the pattern:  (%s ^%y %s)\n", singletonTypeToString(id_type), attrSym, singletonTypeToString(value_type));
        singletons->erase(attrSym);
        attrSym->sc->singleton.possible = false;
        thisAgent->symbolManager->symbol_remove_ref(&attrSym);
    }

    return returnVal;
}
void Explanation_Based_Chunker::clear_singletons()
{
    Symbol* lSym;
    for (auto it = singletons->begin(); it != singletons->end(); ++it)
    {
        lSym = (*it);
        lSym->sc->singleton.possible = false;
        thisAgent->symbolManager->symbol_remove_ref(&lSym);
    }
    singletons->clear();
}

void Explanation_Based_Chunker::add_to_singletons(wme* pWME)
{
    pWME->singleton_status_checked = true;
    pWME->is_singleton = true;
}

const char* TorF(bool isTrue) { if (isTrue) return "true"; else return "false"; }
const char* PassorFail(bool isTrue) { if (isTrue) return "Pass"; else return "Fail"; }

bool Explanation_Based_Chunker::wme_is_a_singleton(wme* pWME)
{
    if (pWME->singleton_status_checked) return pWME->is_singleton;
    if (!pWME->attr->is_string() || !pWME->attr->sc->singleton.possible || !ebc_settings[SETTING_EBC_USER_SINGLETONS]) return false;

    /* This WME has a valid singleton attribute but has never had it's identifier and
     * value elements checked, so we see if it matches the pattern defined in the attribute. */
    bool lIDPassed = false;
    bool lValuePassed = false;
    singleton_element_type id_type = pWME->attr->sc->singleton.id_type;
    singleton_element_type value_type = pWME->attr->sc->singleton.value_type;

//    dprint(DT_DEBUG, "(%y ^%y %y) vs [%s ^%y %s] :", pWME->id, pWME->attr, pWME->value, singletonTypeToString(id_type), pWME->attr, singletonTypeToString(value_type));
    lIDPassed =     ((id_type == ebc_any) ||
                    ((id_type == ebc_identifier) && !pWME->id->is_state() && !pWME->id->is_operator()) ||
                    ((id_type == ebc_state)      && pWME->id->is_state()) ||
                    ((id_type == ebc_operator)   && pWME->id->is_operator()));
    lValuePassed =  ((value_type == ebc_any) ||
                    ((value_type == ebc_state)      && pWME->value->is_state()) ||
                    ((value_type == ebc_identifier) && pWME->value->is_sti() && !pWME->value->is_state() && !pWME->value->is_operator()) ||
                    ((value_type == ebc_constant)   && pWME->value->is_constant()) ||
                    ((value_type == ebc_operator)   && pWME->value->is_operator()));

    pWME->is_singleton = lIDPassed && lValuePassed;
    pWME->singleton_status_checked = true;
//    dprint_noprefix(DT_DEBUG, "%s! = %s + %s\n", PassorFail(pWME->is_singleton), TorF(lIDPassed), TorF(lValuePassed));
    return pWME->is_singleton;
}
