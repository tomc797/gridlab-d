/** $Id: sync_ctrl.cpp
    Implements sychronization control functionality for the sychronization check function.
	Copyright (C) 2020 Battelle Memorial Institute
**/

#include "sync_ctrl.h"

#include <iostream>
#include <cmath>

using namespace std;

static PASSCONFIG clockpass = PC_BOTTOMUP;

//////////////////////////////////////////////////////////////////////////
// sync_ctrl CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
CLASS *sync_ctrl::oclass = NULL;

sync_ctrl::sync_ctrl(MODULE *mod)
{
    if (oclass == NULL)
    {
        oclass = gl_register_class(mod, "sync_ctrl", sizeof(sync_ctrl), PC_PRETOPDOWN | PC_BOTTOMUP | PC_POSTTOPDOWN | PC_AUTOLOCK);
        if (oclass == NULL)
            GL_THROW("unable to register object class implemented by %s", __FILE__);

        if (gl_publish_variable(oclass,
                                //==Flag
                                PT_bool, "armed", PADDR(arm_flag), PT_DESCRIPTION, "Flag to arm the synchronization control functionality.",
                                //==Object
                                PT_object, "sync_check_object", PADDR(sck_obj_pt), PT_DESCRIPTION, "The object reference/name of the sync_check object, which works with this sync_ctrl object.",
                                PT_object, "controlled_generation_unit", PADDR(cgu_obj_pt), PT_DESCRIPTION, "The object reference/name of the controlled generation unit (i.e., a diesel_dg/inverter_dyn object), which serves as the actuator of the PI controllers of this sync_ctrl object.",
                                //==Tolerance
                                PT_double, "frequency_tolerance_ub_Hz[Hz]", PADDR(sct_freq_tol_ub_hz), PT_DESCRIPTION, "The user-specified tolerance in Hz for checking the upper bound of the frequency metric.",
                                PT_double, "frequency_tolerance_lb_Hz[Hz]", PADDR(sct_freq_tol_lb_hz), PT_DESCRIPTION, "The user-specified tolerance in Hz for checking the lower bound of the frequency metric.",
                                PT_double, "voltage_magnitude_tolerance_pu[pu]", PADDR(sct_volt_mag_tol_pu), PT_DESCRIPTION, "The user-specified tolerance in per unit for the difference in voltage magnitudes for checking the voltage metric.",
                                //==Time
                                PT_double, "controlling_period[s]", PADDR(pp_t_ctrl_sec), PT_DESCRIPTION, "The user-defined period when both metrics are satisfied and this sync_ctrl object works in mode A.",
                                PT_double, "monitoring_period[s]", PADDR(pp_t_mon_sec), PT_DESCRIPTION, "The user-defined period when this sync_ctrl object keeps on monitoring in mode B, if both metrics are not violated and the switch object is not closed.",
                                //==Controller
                                PT_double, "PI_Frequency_Kp", PADDR(pi_freq_kp), PT_DESCRIPTION, "The user-defined proportional gain constant of the PI controller for adjusting the frequency setting.",
                                PT_double, "PI_Frequency_Ki", PADDR(pi_freq_ki), PT_DESCRIPTION, "The user-defined integral gain constant of the PI controller for adjusting the frequency setting.",
                                PT_double, "PI_Volt_Mag_Kp", PADDR(pi_volt_mag_kp), PT_DESCRIPTION, "The user-defined proportional gain constant of the PI controller for adjusting the voltage magnitude setting.",
                                PT_double, "PI_Volt_Mag_Ki", PADDR(pi_volt_mag_ki), PT_DESCRIPTION, "	The user-defined integral gain constant of the PI controller for adjusting the voltage magnitude setting.",
                                NULL) < 1)
            GL_THROW("unable to publish properties in %s", __FILE__);

        if (gl_publish_function(oclass, "interupdate_controller_object", (FUNCTIONADDR)interupdate_sync_ctrl) == NULL)
            GL_THROW("Unable to publish sync_ctrl deltamode function");
    }
}

int sync_ctrl::create(void)
{
    // Default settings
    arm_flag = false; //Start as disabled

    return 1;
}

int sync_ctrl::init(OBJECT *parent)
{
    // data_sanity_check(parent);
    // reg_deltamode();

    return 1;
}

TIMESTAMP sync_ctrl::presync(TIMESTAMP t0, TIMESTAMP t1)
{
    //Does nothing right now - presync not part of the sync list for this object
    return TS_NEVER; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

TIMESTAMP sync_ctrl::sync(TIMESTAMP t0, TIMESTAMP t1)
{
    return TS_NEVER;
}

TIMESTAMP sync_ctrl::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
    return TS_NEVER; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

// Deltamode call
// Module-level call
SIMULATIONMODE sync_ctrl::inter_deltaupdate_sync_ctrl(unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
    return SM_EVENT;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: sync_ctrl
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_sync_ctrl(OBJECT **obj, OBJECT *parent)
{
    try
    {
        *obj = gl_create_object(sync_ctrl::oclass);
        if (*obj != NULL)
        {
            sync_ctrl *my = OBJECTDATA(*obj, sync_ctrl);
            gl_set_parent(*obj, parent);
            return my->create();
        }
        else
            return 0;
    }
    CREATE_CATCHALL(sync_ctrl);
}

EXPORT int init_sync_ctrl(OBJECT *obj)
{
    try
    {
        sync_ctrl *my = OBJECTDATA(obj, sync_ctrl);
        return my->init(obj->parent);
    }
    INIT_CATCHALL(sync_ctrl);
}

/**
* Sync is called when the clock needs to advance on the bottom-up pass (PC_BOTTOMUP)
*
* @param obj the object we are sync'ing
* @param t0 this objects current timestamp
* @param pass the current pass for this sync call
* @return t1, where t1>t0 on success, t1=t0 for retry, t1<t0 on failure
*/
EXPORT TIMESTAMP sync_sync_ctrl(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
    sync_ctrl *pObj = OBJECTDATA(obj, sync_ctrl);
    TIMESTAMP t1 = TS_INVALID;

    try
    {
        switch (pass)
        {
        case PC_PRETOPDOWN:
            t1 = pObj->presync(obj->clock, t0);
            break;
        case PC_BOTTOMUP:
            t1 = pObj->sync(obj->clock, t0);
            break;
        case PC_POSTTOPDOWN:
            t1 = pObj->postsync(obj->clock, t0);
            break;
        default:
            GL_THROW("invalid pass request (%d)", pass);
            break;
        }
        if (pass == clockpass)
            obj->clock = t0;
    }
    SYNC_CATCHALL(sync_ctrl);
    return t1;
}

// Deltamode export
EXPORT SIMULATIONMODE interupdate_sync_ctrl(OBJECT *obj, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
    sync_ctrl *my = OBJECTDATA(obj, sync_ctrl);
    SIMULATIONMODE status = SM_ERROR;
    try
    {
        status = my->inter_deltaupdate_sync_ctrl(delta_time, dt, iteration_count_val);
        return status;
    }
    catch (char *msg)
    {
        gl_error("interupdate_sync_ctrl(obj=%d;%s): %s", obj->id, obj->name ? obj->name : "unnamed", msg);
        return status;
    }
}