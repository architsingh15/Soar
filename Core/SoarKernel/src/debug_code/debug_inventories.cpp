/*
 * debug_inventories.cpp
 *
 *  Created on: Feb 1, 2017
 *      Author: mazzin
 */
#include "debug_inventories.h"

#include "agent.h"
#include "dprint.h"
#include "decide.h"
#include "ebc.h"
#include "instantiation.h"
#include "misc.h"
#include "preference.h"
#include "soar_instance.h"
#include "symbol_manager.h"
#include "output_manager.h"
#include "working_memory.h"

#include <assert.h>
#include <string>

#ifdef DEBUG_TRACE_REFCOUNT_FOR

    static int64_t debug_last_refcount = 0;
    static int64_t debug_last_refcount2 = 0;

    void debug_refcount_change_start(agent* thisAgent, bool twoPart)
    {
        int lSymNum;
        std::string numString;
        numString.push_back(DEBUG_TRACE_REFCOUNT_FOR[1]);
        if (!from_string(lSymNum, std::string(numString)) || (lSymNum < 1) ) assert(false);
        Symbol *sym = thisAgent->symbolManager->find_identifier(DEBUG_TRACE_REFCOUNT_FOR[0], 1);
        if (sym)
        {
            int64_t* last_count = twoPart ? &(debug_last_refcount2) : &(debug_last_refcount);
            (*last_count) = sym->reference_count;
        };
    }
    void debug_refcount_change_end(agent* thisAgent, const char* callerString, bool twoPart)
    {
        int lSymNum;
        std::string numString;
        numString.push_back(DEBUG_TRACE_REFCOUNT_FOR[1]);
        if (!from_string(lSymNum, std::string(numString)) || (lSymNum < 1) ) assert(false);
        Symbol *sym = thisAgent->symbolManager->find_identifier(DEBUG_TRACE_REFCOUNT_FOR[0], 1);
        if (sym)
        {
            int64_t new_count = static_cast<int64_t>(sym->reference_count);
            int64_t* last_count = twoPart ? &(debug_last_refcount2) : &(debug_last_refcount);
            if (new_count != (*last_count))
            {
                dprint_noprefix(DT_ID_LEAKING, "%s Reference count of %s changed (%d -> %d) by %d\n", callerString, DEBUG_TRACE_REFCOUNT_FOR,
                    (*last_count), new_count, (new_count - (*last_count)));
            }
            (*last_count) = 0;
            if (twoPart) debug_last_refcount2 = debug_last_refcount2 + (new_count - debug_last_refcount);
        };
    }
    void debug_refcount_reset()
    {
        debug_last_refcount = 0;
        debug_last_refcount2 = 0;
    }

#else
    void debug_refcount_change_start(agent* thisAgent, bool twoPart) {}
    void debug_refcount_change_end(agent* thisAgent, const char* callerString, bool twoPart) {}
    void debug_refcount_reset() {}
#endif

#ifdef DEBUG_GDS_INVENTORY
    id_to_string_map gds_deallocation_map;

    uint64_t GDI_id_counter = 0;
    bool     GDI_double_deallocation_seen = false;

    void GDI_add(agent* thisAgent, goal_dependency_set* pGDS)
    {
        std::string lPrefString;
        pGDS->g_id = ++GDI_id_counter;
        thisAgent->outputManager->sprinta_sf(thisAgent, lPrefString, "GDS %u (%y)", pGDS->g_id, pGDS->goal);
        gds_deallocation_map[pGDS->g_id] = lPrefString;
    }
    void GDI_remove(agent* thisAgent, goal_dependency_set* pGDS)
    {
        auto it = gds_deallocation_map.find(pGDS->g_id);
        assert (it != gds_deallocation_map.end());

        std::string lPrefString = it->second;
        if (!lPrefString.empty())
        {
            gds_deallocation_map[pGDS->g_id].clear();
        } else {
            thisAgent->outputManager->printa_sf(thisAgent, "GDS %u was deallocated twice!\n", it->first);
            break_if_bool(true);
            GDI_double_deallocation_seen = true;
        }
    }
    void GDI_print_and_cleanup(agent* thisAgent)
    {
        std::string lPrefString;
        uint64_t bugCount = 0;
        thisAgent->outputManager->printa_sf(thisAgent, "GDS inventory:            ");
        for (auto it = gds_deallocation_map.begin(); it != gds_deallocation_map.end(); ++it)
        {
            lPrefString = it->second;
            if (!lPrefString.empty())
            {
                bugCount++;
            }
        }
        if (bugCount)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%u/%u were not deallocated", bugCount, GDI_id_counter);
            if (GDI_double_deallocation_seen)
                thisAgent->outputManager->printa_sf(thisAgent, " and some GDSs were deallocated twice");
            if (bugCount <= 23)
                thisAgent->outputManager->printa_sf(thisAgent, ":");
            else
                thisAgent->outputManager->printa_sf(thisAgent, "!\n");
        }
        else if (GDI_id_counter)
            thisAgent->outputManager->printa_sf(thisAgent, "All %u GDSs were deallocated properly.\n", GDI_id_counter);
        else
            thisAgent->outputManager->printa_sf(thisAgent, "No GDSs were created.\n");

        if (bugCount && (bugCount <= 23))
        {
            for (auto it = gds_deallocation_map.begin(); it != gds_deallocation_map.end(); ++it)
            {
                lPrefString = it->second;
                if (!lPrefString.empty()) thisAgent->outputManager->printa_sf(thisAgent, " %s", lPrefString.c_str());
            }
            thisAgent->outputManager->printa_sf(thisAgent, "\n");
        }
        if (((bugCount > 0) || GDI_double_deallocation_seen) && Soar_Instance::Get_Soar_Instance().was_run_from_unit_test())
        {
            std::cout << "Identity set inventory failure.  Leaked identity sets detected.\n";
            assert(false);
        }
        GDI_double_deallocation_seen = false;
        gds_deallocation_map.clear();
        GDI_id_counter = 0;
    }
#else
    void GDI_add(agent* thisAgent, goal_dependency_set* pGDS) {}
    void GDI_remove(agent* thisAgent, goal_dependency_set* pGDS) {}
    void GDI_print_and_cleanup(agent* thisAgent) {}
#endif

#ifdef DEBUG_INSTANTIATION_INVENTORY
    id_to_string_map IDI_deallocation_map;
    bool             IDI_double_deallocation_seen = false;

    void IDI_add(agent* thisAgent, instantiation* pInst)
    {
        std::string lInstString;
        thisAgent->outputManager->sprinta_sf(thisAgent, lInstString, "(%y) in %y (%d)", pInst->prod_name, pInst->match_goal, static_cast<int64_t>(pInst->match_goal_level));
        IDI_deallocation_map[pInst->i_id] = lInstString;
    }
    void IDI_remove(agent* thisAgent, uint64_t pID)
    {
        auto it = IDI_deallocation_map.find(pID);
        assert (it != IDI_deallocation_map.end());

        std::string lInstString = it->second;
        if (!lInstString.empty())
        {
            IDI_deallocation_map[pID].clear();
        } else {
            std::string lInstString;
            thisAgent->outputManager->sprinta_sf(thisAgent, lInstString, "Instantiation %u was deallocated twice!\n", it->first);
            IDI_deallocation_map[pID] = lInstString;
            break_if_bool(true);
            IDI_double_deallocation_seen = true;
        }
    }
    void IDI_print_and_cleanup(agent* thisAgent)
    {
        std::string lInstString;
        uint64_t bugCount = 0;

        thisAgent->outputManager->printa_sf(thisAgent, "Instantiation inventory:  ");
        for (auto it = IDI_deallocation_map.begin(); it != IDI_deallocation_map.end(); ++it)
        {
            lInstString = it->second;
            if (!lInstString.empty())
            {
                bugCount++;
            }
        }
        if (bugCount)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%u/%u were not deallocated", bugCount, IDI_deallocation_map.size());
            if (IDI_double_deallocation_seen)
                thisAgent->outputManager->printa_sf(thisAgent, " and some instantiations sets were deallocated twice");
            if (bugCount <= 23)
                thisAgent->outputManager->printa_sf(thisAgent, ":");
            else
                thisAgent->outputManager->printa_sf(thisAgent, "!\n");
        }
        else if (IDI_deallocation_map.size())
            thisAgent->outputManager->printa_sf(thisAgent, "All %u instantiations were deallocated properly.\n", IDI_deallocation_map.size());
        else
            thisAgent->outputManager->printa_sf(thisAgent, "No instantiations were created.\n");

        if (bugCount && (bugCount <= 23))
        {
            for (auto it = IDI_deallocation_map.begin(); it != IDI_deallocation_map.end(); ++it)
            {
                lInstString = it->second;
                if (!lInstString.empty()) thisAgent->outputManager->printa_sf(thisAgent, " %s", lInstString.c_str());
            }
            thisAgent->outputManager->printa_sf(thisAgent, "\n");
        }
        if (((bugCount > 0) || IDI_double_deallocation_seen) && Soar_Instance::Get_Soar_Instance().was_run_from_unit_test())
        {
            std::cout << "instantiation inventory failure.  Leaked instantiations detected.\n";
            assert(false);
        }
        IDI_double_deallocation_seen = false;
        IDI_deallocation_map.clear();
    }
#else
    void IDI_add(agent* thisAgent, instantiation* pInst) {}
    void IDI_remove(agent* thisAgent, uint64_t pID) {}
    void IDI_print_and_cleanup(agent* thisAgent) {}
#endif

#ifdef DEBUG_PREFERENCE_INVENTORY
    id_to_string_map pref_deallocation_map;

    uint64_t PDI_id_counter = 0;
    bool     PDI_double_deallocation_seen = false;

    void PDI_add(agent* thisAgent, preference* pPref, bool isShallow)
    {
        std::string lPrefString;
        pPref->p_id = ++PDI_id_counter;
        thisAgent->outputManager->sprinta_sf(thisAgent, lPrefString, "%u: %p", pPref->p_id, pPref);
        pref_deallocation_map[pPref->p_id] = lPrefString;
    }
    void PDI_remove(agent* thisAgent, preference* pPref)
    {
        auto it = pref_deallocation_map.find(pPref->p_id);
        assert (it != pref_deallocation_map.end());

        std::string lPrefString = it->second;
        if (!lPrefString.empty())
        {
            pref_deallocation_map[pPref->p_id].clear();
        } else {
            thisAgent->outputManager->printa_sf(thisAgent, "Preferences %u was deallocated twice!\n", it->first);
            break_if_bool(true);
            PDI_double_deallocation_seen = true;
        }
    }

    void PDI_print_and_cleanup(agent* thisAgent)
    {
        std::string lPrefString;
        uint64_t bugCount = 0;
        thisAgent->outputManager->printa_sf(thisAgent, "Preference inventory:     ");
        for (auto it = pref_deallocation_map.begin(); it != pref_deallocation_map.end(); ++it)
        {
            lPrefString = it->second;
            if (!lPrefString.empty())
            {
                bugCount++;
            }
        }
        if (bugCount)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%u/%u were not deallocated", bugCount, PDI_id_counter);
            if (PDI_double_deallocation_seen)
                thisAgent->outputManager->printa_sf(thisAgent, " and some preferences were deallocated twice");
            if (bugCount <= 23)
                thisAgent->outputManager->printa_sf(thisAgent, ":");
            else
                thisAgent->outputManager->printa_sf(thisAgent, "!\n");
        }
        else if (PDI_id_counter)
            thisAgent->outputManager->printa_sf(thisAgent, "All %u preferences were deallocated properly.\n", PDI_id_counter);
        else
            thisAgent->outputManager->printa_sf(thisAgent, "No preferences were created.\n");

        if (bugCount && (bugCount <= 23))
        {
            for (auto it = pref_deallocation_map.begin(); it != pref_deallocation_map.end(); ++it)
            {
                lPrefString = it->second;
                if (!lPrefString.empty()) thisAgent->outputManager->printa_sf(thisAgent, " %s", lPrefString.c_str());
            }
            thisAgent->outputManager->printa_sf(thisAgent, "\n");
        }
        if (((bugCount > 0) || PDI_double_deallocation_seen) && Soar_Instance::Get_Soar_Instance().was_run_from_unit_test())
        {
            std::cout << "Preference inventory failure.  Leaked preferences detected.\n";
            assert(false);
        }
        PDI_double_deallocation_seen = false;
        pref_deallocation_map.clear();
        PDI_id_counter = 0;
    }
#else
    void PDI_add(agent* thisAgent, preference* pPref, bool isShallow) {}
    void PDI_remove(agent* thisAgent, preference* pPref) {}
    void PDI_print_and_cleanup(agent* thisAgent) {}
#endif

#ifdef DEBUG_WME_INVENTORY
    id_to_string_map wme_deallocation_map;

    uint64_t WDI_id_counter = 0;
    bool     WDI_double_deallocation_seen = false;

    void WDI_add(agent* thisAgent, wme* pWME)
    {
        std::string lWMEString;
        pWME->w_id = ++WDI_id_counter;
        thisAgent->outputManager->sprinta_sf(thisAgent, lWMEString, "%u: %w", pWME->w_id, pWME);
        wme_deallocation_map[pWME->w_id] = lWMEString;
    }
    void WDI_remove(agent* thisAgent, wme* pWME)
    {
        auto it = wme_deallocation_map.find(pWME->w_id);
        assert (it != wme_deallocation_map.end());

        std::string lPrefString = it->second;
        if (!lPrefString.empty())
        {
            wme_deallocation_map[pWME->w_id].clear();
        } else {
            thisAgent->outputManager->printa_sf(thisAgent, "WME %u was deallocated twice!\n", it->first);
            break_if_bool(true);
            WDI_double_deallocation_seen = true;
        }
    }

    void WDI_print_and_cleanup(agent* thisAgent)
    {
        std::string lWMEString;
        uint64_t bugCount = 0;
        thisAgent->outputManager->printa_sf(thisAgent, "WME inventory:            ");
        for (auto it = wme_deallocation_map.begin(); it != wme_deallocation_map.end(); ++it)
        {
            lWMEString = it->second;
            if (!lWMEString.empty())
            {
                bugCount++;
            }
        }
        if (bugCount)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%u/%u were not deallocated", bugCount, WDI_id_counter);
            if (WDI_double_deallocation_seen)
                thisAgent->outputManager->printa_sf(thisAgent, " and some WMEs were deallocated twice");
            if (bugCount <= 23)
                thisAgent->outputManager->printa_sf(thisAgent, ":");
            else
                thisAgent->outputManager->printa_sf(thisAgent, "!\n");
        }
        else if (WDI_id_counter)
            thisAgent->outputManager->printa_sf(thisAgent, "All %u WMEs were deallocated properly.\n", WDI_id_counter);
        else
            thisAgent->outputManager->printa_sf(thisAgent, "No WMEs were created.\n");

        if (bugCount && (bugCount <= 23))
        {
            for (auto it = wme_deallocation_map.begin(); it != wme_deallocation_map.end(); ++it)
            {
                lWMEString = it->second;
                if (!lWMEString.empty()) thisAgent->outputManager->printa_sf(thisAgent, " %s", lWMEString.c_str());
            }
            thisAgent->outputManager->printa_sf(thisAgent, "\n");
        }
        if (((bugCount > 0) || WDI_double_deallocation_seen) && Soar_Instance::Get_Soar_Instance().was_run_from_unit_test())
        {
            std::cout << "WME inventory failure.  Leaked WMEs detected.\n";
            assert(false);
        }
        WDI_double_deallocation_seen = false;
        wme_deallocation_map.clear();
        WDI_id_counter = 0;
    }
#else
    void WDI_add(agent* thisAgent, wme* pWME) {}
    void WDI_remove(agent* thisAgent, wme* pWME) {}
    void WDI_print_and_cleanup(agent* thisAgent) {}
#endif

#ifdef DEBUG_IDSET_INVENTORY
    id_to_string_map idset_deallocation_map;

    bool     ISI_double_deallocation_seen = false;

    void ISI_add(agent* thisAgent, uint64_t pIDSetID)
    {
        std::string lPrefString;
        thisAgent->outputManager->sprinta_sf(thisAgent, lPrefString, "%u", pIDSetID);
        idset_deallocation_map[pIDSetID].assign(lPrefString);
    }
    void ISI_remove(agent* thisAgent, uint64_t pIDSetID)
    {
        auto it = idset_deallocation_map.find(pIDSetID);
        assert (it != idset_deallocation_map.end());

        std::string lPrefString = it->second;
        if (!lPrefString.empty())
        {
            idset_deallocation_map[pIDSetID].clear();
        } else {
            thisAgent->outputManager->printa_sf(thisAgent, "Identity set %u was deallocated twice!\n", it->first);
            break_if_bool(true);
            ISI_double_deallocation_seen = true;
        }
    }
    void ISI_print_and_cleanup(agent* thisAgent)
    {
        std::string lPrefString;
        uint64_t bugCount = 0;
        thisAgent->outputManager->printa_sf(thisAgent, "Identity set inventory:   ");
        for (auto it = idset_deallocation_map.begin(); it != idset_deallocation_map.end(); ++it)
        {
            lPrefString = it->second;
            if (!lPrefString.empty())
            {
                bugCount++;
            }
        }
        if (bugCount)
        {
            thisAgent->outputManager->printa_sf(thisAgent, "%u/%d were not deallocated", bugCount, static_cast<int64_t>(idset_deallocation_map.size()));
            if (ISI_double_deallocation_seen)
                thisAgent->outputManager->printa_sf(thisAgent, " and some identity sets were deallocated twice");
            if (bugCount <= 23)
                thisAgent->outputManager->printa_sf(thisAgent, ":");
            else
                thisAgent->outputManager->printa_sf(thisAgent, "!\n");
        }
        else if (idset_deallocation_map.size())
            thisAgent->outputManager->printa_sf(thisAgent, "All %d identity sets were deallocated properly.\n", static_cast<int64_t>(idset_deallocation_map.size()));
        else
            thisAgent->outputManager->printa_sf(thisAgent, "No identity sets were created.\n");

        if (bugCount && (bugCount <= 23))
        {
            for (auto it = idset_deallocation_map.begin(); it != idset_deallocation_map.end(); ++it)
            {
                lPrefString = it->second;
                if (!lPrefString.empty()) thisAgent->outputManager->printa_sf(thisAgent, " %s", lPrefString.c_str());
            }
            thisAgent->outputManager->printa_sf(thisAgent, "\n");
        }
        if (((bugCount > 0) || ISI_double_deallocation_seen) && Soar_Instance::Get_Soar_Instance().was_run_from_unit_test())
        {
            std::cout << "Identity set inventory failure.  Leaked identity sets detected.\n";
            assert(false);
        }
        ISI_double_deallocation_seen = false;
        idset_deallocation_map.clear();
    }
#else
    void ISI_add(agent* thisAgent, uint64_t pIDSetID) {}
    void ISI_remove(agent* thisAgent, uint64_t pIDSetID) {}
    void ISI_print_and_cleanup(agent* thisAgent) {}
#endif
