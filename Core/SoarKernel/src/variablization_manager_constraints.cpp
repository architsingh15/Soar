/*
 * variablization_manager_constraints.cpp
 *
 *  Created on: Jul 25, 2013
 *      Author: mazzin
 */

#include "variablization_manager.h"
#include "agent.h"
#include "instantiations.h"
#include "assert.h"
#include "test.h"
#include "print.h"
#include "debug.h"

/*    Note: We do not need to deallocate the symbol key in any of the following code b/c we did not
 *          increase the refcount on those symbols.  We do need to deallocate the test b/c it
 *          was a copy of another test in another instantiation. */
void Variablization_Manager::variablize_relational_constraints()
{
    variablization *found_variablization = NULL;
    ::list *c;
    std::map< Symbol *, ::list * > *variablized_sti_constraints = new std::map< Symbol *, ::list * >;
    std::map< uint64_t, ::list * > *variablized_constant_constraints = new std::map< uint64_t, ::list * >;

    dprint(DT_CONSTRAINTS, "=============================================\n");
    dprint(DT_CONSTRAINTS, "Variablizing relational constraints.\n");
    dprint(DT_CONSTRAINTS, "(1) Variablizing relational constraints for short-term identifiers.\n");
    dprint_wmes(DT_CONSTRAINTS, true);


    /* -- Replace sti constraints with variablized version and delete any ungrounded tests and
     *    any constraints whose symbol key has not been variablized during the equality
     *    variablization pass. -- */
    for (std::map< Symbol *, ::list * >::iterator it=sti_constraints->begin(); it!=sti_constraints->end(); ++it)
    {

        dprint(DT_CONSTRAINTS, "Looking for variablization for equality symbol %s.\n", it->first->to_string());
        found_variablization = get_variablization(it->first);

        if (found_variablization)
        {
            // Should always be grounded now that relationals are done on their own
            assert(found_variablization->grounded);

            dprint(DT_CONSTRAINTS, "...found grounding.  Variablizing constraint list.\n", it->first->to_string());

            variablize_relational_constraints_for_symbol(&(it->second));

            /* -- If at least one relational constraint remains in the list, add to variablized constraint
             *    list, using the variablized equality symbol -- */
            if (it->second)
            {
                dprint(DT_CONSTRAINTS, "...variablized constraints exist.  Copying to new constraint list.\n");
                (*variablized_sti_constraints)[found_variablization->variablized_symbol] = it->second;
            }
        }
        else
        {
            /* -- Delete entire constraint list for ungrounded identifier */
            dprint(DT_CONSTRAINTS, "...not variablizing constraint list b/c equality symbol not in chunk.  Deallocating tests.\n");
            c = it->second;
            while (c)
            {
                dprint(DT_CONSTRAINTS, "...deallocating test %s\n", test_to_string(static_cast<test>(c->first)));
                deallocate_test(thisAgent, static_cast<test>(c->first));
                c = c->rest;
            }
            free_list (thisAgent, it->second);
            it->second = NULL;
        }
    }

    sti_constraints->clear();
    sti_constraints = variablized_sti_constraints;

    /* -- Replace constant constraints with variablized version.  Delete only constraints whose symbol key
     *    has not been variablized during the equality variablization pass. -- */
    dprint(DT_CONSTRAINTS, "(2) Variablizing relational constraints for constant symbols.\n");
    for (std::map< uint64_t, ::list * >::iterator it=constant_constraints->begin(); it!=constant_constraints->end(); ++it)
    {

        dprint(DT_CONSTRAINTS, "Looking for variablization for equality g_id %llu.\n", it->first);
        found_variablization = get_variablization(it->first);

        if (found_variablization)
        {
            // Should always be grounded now that relationals are done on their own
            assert(found_variablization->grounded);

            dprint(DT_CONSTRAINTS, "...found grounding for grounding id %llu.  Variablizing constraint list.\n", it->first);
            variablize_relational_constraints_for_symbol(&(it->second));

            /* -- If at least one relational constraint remains in the list, add to variablized constraint
             *    list, using the variablized equality symbol -- */
            /* MToDo | Don't think this is possible for constants  Checking.  Remove */
            assert(it->second);
            if (it->second)
            {
                dprint(DT_CONSTRAINTS, "...variablized constraints exist.  Copying to new constraint list.\n");
                (*variablized_constant_constraints)[found_variablization->grounding_id] = it->second;
            }
        } else {
            /* -- Delete entire constraint list for ungrounded identifier -- */
            dprint(DT_CONSTRAINTS, "...not variablizing constraint list b/c equality g_id not in chunk.  Deallocating tests.\n");
            c = it->second;
            while (c)
            {
                dprint(DT_CONSTRAINTS, "...deallocating test %s\n", test_to_string(static_cast<test>(c->first)));
                deallocate_test(thisAgent, static_cast<test>(c->first));
                c = c->rest;
            }
            free_list (thisAgent, it->second);
            it->second = NULL;
        }
    }

    /* -- Replace constant constraints with variablized version. -- */
    constant_constraints->clear();
    constant_constraints = variablized_constant_constraints;

    dprint(DT_CONSTRAINTS, "Done variablizing relational constraints.\n");

}

void Variablization_Manager::variablize_relational_constraints_for_symbol(::list **constraint_list)
{
    variablization *found_variablization = NULL;
    cons *c, *c_next, *c_last;
    test t;
    bool success;

    /* MToDo | Remove.  Quick check to see if it is hanging here somehow. */
    int64_t debug_count=0;

    c = (*constraint_list);
    c_last = NULL;
    while (c)
    {
        assert(++debug_count<100);
        c_next = c->rest;
        t = static_cast<test>(c->first);
        // Should not be possible to be a conjunctive test
        assert(t->type != CONJUNCTIVE_TEST);

        success = variablize_test_by_lookup(&(t), false);
        if (!success)
        {
            /* -- STI identifier that is ungrounded.  Delete. -- */
            dprint(DT_CONSTRAINTS, "Deleting constraint b/c STI not in in chunk.\n");
            if (c_last)
            {
                /* -- Not at the head of the list -- */
                c_last->rest = c->rest;
                free_cons(thisAgent, c);
                c = c_last;
            } else {
                /* -- At the head of the list -- */
                (*constraint_list) = c->rest;
                free_cons(thisAgent, c);
                /* -- This will cause c_last to be set to NULL, indicating we're
                 *    at the head of the list -- */
                c = NULL;
            }
            deallocate_test(thisAgent, t);
        }
        c_last = c;
        c = c_next;
    }
}

void Variablization_Manager::clear_relational_constraints ()
{
    for (std::map< Symbol *, ::list * >::iterator it=sti_constraints->begin(); it!=sti_constraints->end(); ++it)
    {
        free_list (thisAgent, it->second);
    }
    sti_constraints->clear();

    for (std::map< uint64_t, ::list * >::iterator it=constant_constraints->begin(); it!=constant_constraints->end(); ++it)
    {
        free_list (thisAgent, it->second);
    }
    constant_constraints->clear();
}

void Variablization_Manager::cache_relational_constraint (test equality_test, test relational_test)
{
    dprint(DT_CONSTRAINTS, "Adding relational constraint %s to %s.\n", test_to_string(relational_test), test_to_string(equality_test));
    ::list * new_list=NULL;
    test copied_test = copy_test(thisAgent, relational_test);

    if (equality_test->data.referent->is_sti())
    {
        std::map< Symbol *, ::list * >::iterator iter = (*sti_constraints).find(equality_test->data.referent);
        if (iter == sti_constraints->end())
        {
            push(thisAgent, (copied_test), new_list);
            (*sti_constraints)[equality_test->data.referent] = new_list;
            dprint(DT_CONSTRAINTS, "ADDED (*sti_constraints)[%s] + %s\n", equality_test->data.referent->to_string(), test_to_string(copied_test));
        }
        else
        {
            new_list = (*sti_constraints)[equality_test->data.referent];
            push(thisAgent, (copied_test), new_list);
            (*sti_constraints)[equality_test->data.referent] = new_list;
            dprint(DT_CONSTRAINTS, "ADDED (*sti_constraints)[%s] + %s\n", equality_test->data.referent->to_string(), test_to_string(copied_test));
        }
    } else {
        std::map< uint64_t, ::list * >::iterator iter = (*constant_constraints).find(equality_test->identity->grounding_id);
        if (iter == constant_constraints->end())
        {
            push(thisAgent, (copied_test), new_list);
            (*constant_constraints)[equality_test->identity->grounding_id] = new_list;
            dprint(DT_CONSTRAINTS, "ADDED (*constant_constraints)[%llu] + %s\n", equality_test->identity->grounding_id, test_to_string(copied_test));
        }
        else
        {
            new_list = (*constant_constraints)[equality_test->identity->grounding_id];
            push(thisAgent, (copied_test), new_list);
            (*constant_constraints)[equality_test->identity->grounding_id] = new_list;
            dprint(DT_CONSTRAINTS, "ADDED (*constant_constraints)[%llu] + %s\n", equality_test->identity->grounding_id, test_to_string(copied_test));
        }
    }
}

void Variablization_Manager::cache_relational_constraints_in_test (test t)
{
    /* -- Only conjunctive tests can have relational tests here.  Otherwise,
     *    should be an equality test. -- */
    if (t->type != CONJUNCTIVE_TEST)
    {
        assert(t->type == EQUALITY_TEST);
        return;
    }

    test equality_test=NULL, referent_test, ctest;
    cons *c;
    for (c=t->data.conjunct_list; c!=NIL; c=c->rest)
    {
        if (static_cast<test>(c->first)->type == EQUALITY_TEST)
        {
            equality_test = static_cast<test>(c->first);
            break;
        }
    }
    assert(equality_test);
    for (c=t->data.conjunct_list; c!=NIL; c=c->rest)
    {
        ctest = static_cast<test>(c->first);
        switch (ctest->type) {
            case EQUALITY_TEST:
                break;
            case GREATER_TEST:
            case GREATER_OR_EQUAL_TEST:
            case LESS_TEST:
            case LESS_OR_EQUAL_TEST:
            case NOT_EQUAL_TEST:
            case SAME_TYPE_TEST:
            case DISJUNCTION_TEST:
                cache_relational_constraint(equality_test, ctest);
                break;
            default:
                break;
        }
    }
}

void Variablization_Manager::cache_relational_constraints_in_cond (condition *c)
{
    /* Don't need to do id element.  It should always be an equality test */
    //  assert(!c->data.tests.id_test || (c->data.tests.id_test->type == EQUALITY_TEST));
    dprint_condition(DT_CONSTRAINTS, c, "Caching relational constraints in condition: ", true, false, true);
    cache_relational_constraints_in_test(c->data.tests.attr_test);
    cache_relational_constraints_in_test(c->data.tests.value_test);
}

void Variablization_Manager::install_relational_constraints_for_test(test *t)
{
    if (!t) return;

    cons *c;
    test eq_test, ct;
    Symbol *eq_symbol;
    variablization *found_variablization;

    eq_test = equality_test_found_in_test(thisAgent, *t);
    assert(eq_test);
    eq_symbol = eq_test->data.referent;
    dprint(DT_CONSTRAINTS, "Calling add_relational_constraints_for_test() for symbol %s(%llu).\n", eq_symbol->to_string(), eq_test->identity ? eq_test->identity->grounding_id : 0);
    if (!eq_test->identity || (eq_test->identity->grounding_id == 0))
    {
        /* MToDo | Could also just use was_identifier, though that might not be needed now that we don't reverse */
        assert(eq_symbol->is_variable() && eq_symbol->var->was_identifier);
        dprint(DT_CONSTRAINTS, "...no identity, so must be STI.  Using symbol to look up.\n");
        found_variablization = get_variablization(eq_symbol);
        if (found_variablization)
        {
            dprint(DT_CONSTRAINTS, "...variablization found.  Variablized symbol = %s.\n", found_variablization->variablized_symbol->to_string());
            print_relational_constraints(DT_CONSTRAINTS);
            std::map< Symbol *, ::list * >::iterator iter = (*sti_constraints).find(eq_symbol);
            if (iter != (*sti_constraints).end())
            {
                dprint(DT_CONSTRAINTS, "...adding relational constraint list for symbol %s...\n", eq_symbol->to_string());
                c = iter->second;
                while (c) {
                    ct = static_cast<test>(c->first);
                    dprint_test(DT_CONSTRAINTS, ct, true, false, true, "...adding", "\n");
                    add_test(thisAgent, t, ct);
                    c = c->rest;
                }
                free_list (thisAgent, iter->second);
                (*sti_constraints).erase(iter->first);
            }
            else
            {
                dprint(DT_CONSTRAINTS, "...no relational constraints found.\n");
            }
        } else {
            dprint(DT_CONSTRAINTS, "... was never variablized. Skipping...\n");
        }
    } else {
        dprint(DT_CONSTRAINTS, "...identity, so must be constant.  Using g_id to look up.\n");
        found_variablization = get_variablization(eq_test->identity->grounding_id);
        if (found_variablization)
        {
            dprint(DT_CONSTRAINTS, "...variablization found.  Variablized symbol = %s.\n", found_variablization->variablized_symbol->to_string());
            print_relational_constraints(DT_CONSTRAINTS);
            std::map< uint64_t, ::list * >::iterator iter = (*constant_constraints).find(eq_test->identity->grounding_id);
            if (iter != (*constant_constraints).end())
            {
                dprint(DT_CONSTRAINTS, "...adding relational constraint list for symbol %s...\n", eq_symbol->to_string());
                c = iter->second;
                while (c) {
                    ct = static_cast<test>(c->first);
                    dprint_test(DT_CONSTRAINTS, ct, true, false, true, "...adding ", "\n");
                    add_test(thisAgent, t, ct);
                    c = c->rest;
                }
                free_list (thisAgent, iter->second);
                (*constant_constraints).erase(iter->first);
            }
            else
            {
                dprint(DT_CONSTRAINTS, "...no relational constraints found.\n");
            }
        } else {
            dprint(DT_CONSTRAINTS, "... was never variablized. Skipping...\n");
        }
    }
}

void Variablization_Manager::install_relational_constraints(condition *cond)
{
    dprint(DT_CONSTRAINTS, "=============================================\n");
    dprint(DT_CONSTRAINTS, "install_relational_constraints called...\n");
    print_variablization_tables(DT_CONSTRAINTS);
    print_relational_constraints(DT_CONSTRAINTS);

  /* MToDo | Vast majority of constraints will be on value element.  Making this work with a pass for
   *         values followed by attributes could be faster. */

    while (cond && ((sti_constraints->size() > 0) || (constant_constraints->size() > 0))) {
        if (cond->type == POSITIVE_CONDITION)
        {
            dprint(DT_CONSTRAINTS, "Adding for positive condition ");
            dprint_condition(DT_CONSTRAINTS, cond, "", true, false, true);
            install_relational_constraints_for_test(&cond->data.tests.attr_test);
            install_relational_constraints_for_test(&cond->data.tests.value_test);
        } else {
            dprint(DT_CONSTRAINTS, (cond->type == NEGATIVE_CONDITION) ? "Skipping for negative condition " : "Skipping for negative conjunctive condition:\n");
            dprint_condition(DT_CONSTRAINTS, cond, "", true, false, true);
        }
        cond = cond->next;
    }
    dprint(DT_CONSTRAINTS, "install_relational_constraints done adding constraints.  Final tables:\n");
    print_variablization_tables(DT_CONSTRAINTS);
    print_relational_constraints(DT_CONSTRAINTS);
    dprint_condition_list(DT_CONSTRAINTS, cond);
    dprint(DT_CONSTRAINTS, "=============================================\n");
}
