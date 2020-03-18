/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-03-10
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "csmonitor.hh"

#include <regex>
#include <vector>
#include <string>
#include <mysql.h>

#include <maxscale/modinfo.hh>
#include <maxscale/mysql_utils.hh>

using maxscale::MonitorServer;

namespace
{

constexpr const char* alive_query = "SELECT mcsSystemReady() = 1 && mcsSystemReadOnly() <> 2";
constexpr const char* role_query = "SELECT mcsSystemPrimary()";

// Helper for extracting string results from queries
static std::string do_query(MonitorServer* srv, const char* query)
{
    std::string rval;
    MYSQL_RES* result;

    if (mxs_mysql_query(srv->con, query) == 0 && (result = mysql_store_result(srv->con)))
    {
        MYSQL_ROW row = mysql_fetch_row(result);

        if (row && row[0])
        {
            rval = row[0];
        }

        mysql_free_result(result);
    }
    else
    {
        srv->mon_report_query_error();
    }

    return rval;
}

// Returns a numeric version similar to mysql_get_server_version
int get_cs_version(MonitorServer* srv)
{
    int rval = -1;
    std::string prefix = "Columnstore ";
    std::string result = do_query(srv, "SELECT @@version_comment");
    auto pos = result.find(prefix);

    if (pos != std::string::npos)
    {
        std::istringstream os(result.substr(pos + prefix.length()));
        int major = 0, minor = 0, patch = 0;
        char dot;
        os >> major;
        os >> dot;
        os >> minor;
        os >> dot;
        os >> patch;
        rval = major * 10000 + minor * 100 + patch;
    }

    return rval;
}
}

CsMonitor::CsMonitor(const std::string& name, const std::string& module)
    : MonitorWorkerSimple(name, module)
    , m_config(name)
{
}

CsMonitor::~CsMonitor()
{
}

// static
CsMonitor* CsMonitor::create(const std::string& name, const std::string& module)
{
    return new CsMonitor(name, module);
}

bool CsMonitor::has_sufficient_permissions()
{
    return test_permissions(alive_query);
}

void CsMonitor::update_server_status(MonitorServer* srv)
{
    srv->clear_pending_status(SERVER_MASTER | SERVER_SLAVE | SERVER_RUNNING);
    int status = 0;

    if (do_query(srv, alive_query) == "1")
    {
        auto version = get_cs_version(srv);

        if (version >= 0)
        {
            status |= SERVER_RUNNING;

            if (version >= 10200)
            {
                // 1.2 supports the mcsSystemPrimary function
                status |= do_query(srv, role_query) == "1" ? SERVER_MASTER : SERVER_SLAVE;
            }
            else
            {
                status |= srv->server == m_config.pPrimary ? SERVER_MASTER : SERVER_SLAVE;
            }
        }
    }

    srv->set_pending_status(status);
}

bool CsMonitor::configure(const mxs::ConfigParameters* pParams)
{
    bool rv = false;

    if (MonitorWorkerSimple::configure(pParams))
    {
        rv = m_config.configure(*pParams);
    }

    return rv;
}

namespace
{

void reject_not_running(const char* zCmd, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput,
                         "The Columnstore monitor is not running, cannot "
                         "execute the command '%s'.", zCmd);
}

void reject_call_failed(const char* zCmd, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "Failed to queue the command '%s' for execution.", zCmd);
}

}

bool CsMonitor::command_cluster_start(json_t** ppOutput)
{
    mxb::Semaphore sem;

    auto cmd = [this, &sem, ppOutput] () {
        cluster_start(sem, ppOutput);
    };

    return command("cluster-start", cmd, sem, ppOutput);
}

bool CsMonitor::command_cluster_stop(json_t** ppOutput)
{
    mxb::Semaphore sem;

    auto cmd = [this, &sem, ppOutput] () {
        cluster_stop(sem, ppOutput);
    };

    return command("cluster-stop", cmd, sem, ppOutput);
}

bool CsMonitor::command_cluster_shutdown(json_t** ppOutput)
{
    mxb::Semaphore sem;

    auto cmd = [this, &sem, ppOutput] () {
        cluster_shutdown(sem, ppOutput);
    };

    return command("cluster-shutdown", cmd, sem, ppOutput);
}

bool CsMonitor::command_cluster_add_node(json_t** ppOutput)
{
    mxb::Semaphore sem;

    auto cmd = [this, &sem, ppOutput] () {
        cluster_add_node(sem, ppOutput);
    };

    return command("cluster-add-node", cmd, sem, ppOutput);
}

bool CsMonitor::command_cluster_remove_node(json_t** ppOutput)
{
    mxb::Semaphore sem;

    auto cmd = [this, &sem, ppOutput] () {
        cluster_remove_node(sem, ppOutput);
    };

    return command("cluster-remove-node", cmd, sem, ppOutput);
}

bool CsMonitor::command(const char* zCmd, std::function<void()> cmd, mxb::Semaphore& sem, json_t** ppOutput)
{
    bool rv = false;

    if (!is_running())
    {
        reject_not_running(zCmd, ppOutput);
    }
    else
    {
        if (execute(cmd, EXECUTE_QUEUED))
        {
            sem.wait();
            rv = true;
        }
        else
        {
            reject_call_failed(zCmd, ppOutput);
        }
    }

    return rv;
}

void CsMonitor::cluster_start(mxb::Semaphore& sem, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "cluster-start not implemented yet.");
    sem.post();
}

void CsMonitor::cluster_stop(mxb::Semaphore& sem, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "cluster-stop not implemented yet.");
    sem.post();
}

void CsMonitor::cluster_shutdown(mxb::Semaphore& sem, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "cluster-shutdown not implemented yet.");
    sem.post();
}

void CsMonitor::cluster_add_node(mxb::Semaphore& sem, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "cluster-add-node not implemented yet.");
    sem.post();
}

void CsMonitor::cluster_remove_node(mxb::Semaphore& sem, json_t** ppOutput)
{
    PRINT_MXS_JSON_ERROR(ppOutput, "cluster-remove-node not implemented yet.");
    sem.post();
}