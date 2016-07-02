/*
 * visualize.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: mazzin
 */
#include "visualize.h"

#include "action_record.h"
#include "condition_record.h"
#include "identity_record.h"
#include "instantiation_record.h"
#include "production_record.h"

#include "agent.h"
#include "condition.h"
#include "ebc.h"
#include "instantiation.h"
#include "output_manager.h"
#include "preference.h"
#include "print.h"
#include "production.h"
#include "rete.h"
#include "rhs.h"
#include "test.h"
#include "working_memory.h"
#include "../debug_code/dprint.h"

GraphViz_Visualizer::GraphViz_Visualizer(agent* myAgent)
{
    thisAgent = myAgent;
    outputManager = thisAgent->outputManager;
    m_viz_print = false;
    m_viz_launch_image = true;
    m_viz_launch_gv = false;
    m_simple_inst = false;
    m_include_arch = false;
    m_use_same_file = true;
    m_generate_img = true;
    m_line_style = "polyline";
    m_filename_prefix = "soar_visualization";
    m_image_type = "svg";
}

GraphViz_Visualizer::~GraphViz_Visualizer()
{
}

void GraphViz_Visualizer::visualize_wm()
{
    graphviz_output.clear();

    WM_Visualization_Map* wme_map = new WM_Visualization_Map(thisAgent);
    viz_graph_start(false);
    if (m_simple_inst)
    {
        wme_map->visualize_wm_as_graph();
    } else {
        wme_map->visualize_wm_as_linked_records();
    }
    viz_graph_end();
    delete wme_map;
}

void GraphViz_Visualizer::viz_graph_start(bool pLeftRight)
{
    graphviz_output.clear();

    graphviz_output +=  "digraph g {\n"
        "   node [shape = \"box\" fontsize = \"16\"];\n"
        "   edge [];\n";
    if (pLeftRight)
    {
        graphviz_output +=  "   graph [ rankdir = \"LR\" ";
    } else {
        graphviz_output +=  "   graph [ rankdir = \"TD\" ";
    }
    graphviz_output +=  "splines = \"";
    graphviz_output += m_line_style;
    graphviz_output +=  "\"];\n";
}

void GraphViz_Visualizer::viz_graph_end()
{
    graphviz_output += "}\n";

    escape_graphviz_chars();
}

void GraphViz_Visualizer::viz_object_start(Symbol* pName, uint64_t node_id, visualizationObjectType objectType)
{
    switch (objectType)
    {
        case viz_inst_record:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   rule%u [\n"
                "      penwidth = \"0\"\n"
                "      label = \xF3", node_id);
            viz_table_start();
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\"\xF2Instantiation (i %u) of rule\xF3\x42R/\xF2%y\xF3/TD\xF2 \xF3/TR\xF2\n", node_id, pName);
            break;

        case viz_chunk_record:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   chunk%u [\n"
                "      style = \"dashed, bold, rounded\"\n"
                "      label = \xF3", node_id);
            viz_table_start();
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\"\xF2%y (i %u)\xF3/TD\xF2 \xF3/TR\xF2\n", pName, node_id);
            break;
        case viz_id_and_augs:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   \"%y\" [\n"
                "      penwidth = \"0\"\n"
                "      label = \xF3", pName);
            viz_table_start();
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\"\xF2%y\xF3/TD\xF2 \xF3/TR\xF2\n", pName);
            break;
        case viz_simple_inst:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   rule%u [\n"
                "      shape = \"box\" style = \"rounded\"\n"
                "      label = \"%y (i %u)", node_id, pName, node_id);
            break;
        case viz_wme:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   \"%y\" [\n"
                "      shape = \"box\" style = \"rounded\"\n"
                "      label = \"%y", pName, pName);
            break;

        case viz_wme_terminal:
            outputManager->sprinta_sf(thisAgent, graphviz_output,
                "   \"%y\" [\n"
                "      shape = \"circle\" style = \"rounded\"\n"
                "      label = \"%y", pName, pName);
            break;

        default:
            assert(false);
            break;
    }

}

void GraphViz_Visualizer::viz_object_end(visualizationObjectType objectType)
{
    switch (objectType)
    {
        case viz_inst_record:
        case viz_chunk_record:
        case viz_id_and_augs:
            viz_table_end();
            graphviz_output += "              \xF2\n   ];\n\n";
            break;

        case viz_simple_inst:
        case viz_wme:
            graphviz_output += "\"\n   ];\n\n";
            break;

        default:
            assert(false);
            break;
    }
}

void GraphViz_Visualizer::viz_record_start()
{
    graphviz_output += "                \xF3TR\xF2 ";
}

void GraphViz_Visualizer::viz_record_end(bool pLeftJustify)
{
    if (pLeftJustify)
        graphviz_output +=  " \xF3/TR\xF2";
    else
        graphviz_output +=  " \xF3/TR\xF2";
}
void GraphViz_Visualizer::viz_table_start()
{
    outputManager->sprinta_sf(thisAgent, graphviz_output,
        "\xF3TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\"\xF2\n");
}

void GraphViz_Visualizer::viz_table_end()
{
    graphviz_output += "              \xF3/TABLE\xF2\n";
}

void GraphViz_Visualizer::viz_table_element_conj_start(uint64_t pNodeID, char pTypeChar, bool pIsLeftPort)
{
    if (pNodeID)
    {
        outputManager->sprinta_sf(thisAgent, graphviz_output, "\n                \xF3TD PORT=\"%c_%u%cs\"\xF2 ",
            pTypeChar, pNodeID, (pIsLeftPort ? "_l" : "_r"));
    } else {
        outputManager->sprinta_sf(thisAgent, graphviz_output, "\n                \xF3TD\xF2 ");
    }
    viz_table_start();
}

void GraphViz_Visualizer::viz_table_element_start(uint64_t pNodeID, char pTypeChar, bool pIsLeftPort)
{
    if (pNodeID)
    {
        outputManager->sprinta_sf(thisAgent, graphviz_output, "\xF3TD PORT=\"%c_%u%s\"\xF2 ",
            pTypeChar, pNodeID, (pIsLeftPort ? "_l" : "_r"));
    } else {
        outputManager->sprinta_sf(thisAgent, graphviz_output, "\xF3TD\xF2 ");
    }
}

void GraphViz_Visualizer::viz_table_element_end()
{
    outputManager->sprinta_sf(thisAgent, graphviz_output, "\xF3/TD\xF2");
}

void GraphViz_Visualizer::viz_text_record(const char* pMsg)
{
    outputManager->sprinta_sf(thisAgent, graphviz_output, "\xF3TD\xF2 %s \xF3/TD\xF2", pMsg);
}

void GraphViz_Visualizer::viz_NCC_start()
{
    graphviz_output +=  "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\" ALIGN=\"LEFT\"\xF2 -\xE3 \xF3/TD\xF2 \xF3/TR\xF2\n";
}

void GraphViz_Visualizer::viz_NCC_end()
{
    graphviz_output +=  "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\" ALIGN=\"LEFT\"\xF2 \xE1 \xF3/TD\xF2 \xF3/TR\xF2\n";
}

void GraphViz_Visualizer::viz_seperator()
{
    graphviz_output +=  "                \xF3TR\xF2 \xF3TD COLSPAN=\"3\"\xF2 ----> \xF3/TD\xF2 \xF3/TR\xF2";
}

void GraphViz_Visualizer::viz_endl()
{
    graphviz_output +=  "\n";
}

void GraphViz_Visualizer::escape_graphviz_chars()
{

    if (graphviz_output.empty()) return;

    char last_char = 0;
    std::string finalString;

    for(char &c : graphviz_output)
    {
        switch (c)
        {
            case '<':
                finalString += "&lt;";
                break;
            case '>':
                finalString += "&gt;";
                break;
            case '\xF3':
                finalString += "<";
                break;
            case '\xF2':
                finalString += ">";
                break;
            case '\xE3':
                finalString += "{";
                break;
            case '\xE1':
                finalString += "}";
                break;
            default:
                if (last_char)
                {
                    finalString += last_char;
                    last_char = 0;
                }
                finalString += c;
                break;
        }
    }
    graphviz_output = finalString;
}

void GraphViz_Visualizer::clear_visualization()
{
        graphviz_output.clear();
}
void GraphViz_Visualizer::viz_connect_action_to_cond(uint64_t pSrcRuleID, uint64_t pSrcActionID, uint64_t pTargetRuleID, uint64_t pTargetCondID)
{
    graphviz_output += "   rule";
    graphviz_output += std::to_string(pSrcRuleID);
    if (thisAgent->visualizer->is_simple_inst_enabled())
    {
        graphviz_output += ":e";
    } else {
        graphviz_output += ":a_";
        graphviz_output += std::to_string(pSrcActionID);
        graphviz_output += "_r ";
    }
    graphviz_output += "-\xF2 rule";
    graphviz_output += std::to_string(pTargetRuleID);
    if (thisAgent->visualizer->is_simple_inst_enabled())
    {
        graphviz_output += ":w\n";
    } else {
        graphviz_output += ":c_";
        graphviz_output += std::to_string(pTargetCondID);
        graphviz_output += "_l\n";
    }
}

void GraphViz_Visualizer::viz_connect_inst_to_chunk(uint64_t pSrcRuleID, uint64_t pTargetRuleID, uint64_t pTargetCondID)
{
    graphviz_output += "   rule";
    graphviz_output += std::to_string(pSrcRuleID);
    graphviz_output += " -\xF2 chunk";
    graphviz_output += std::to_string(pTargetRuleID);
    graphviz_output += ":c_";
    graphviz_output += std::to_string(pTargetCondID);
    graphviz_output += "_l [style = \"dashed\"  penwidth = \"2\"]\n";
}

