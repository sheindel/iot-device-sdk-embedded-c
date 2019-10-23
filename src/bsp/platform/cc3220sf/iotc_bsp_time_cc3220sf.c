/* Copyright 2018-2019 Google LLC
 *
 * This is part of the Google Cloud IoT Device SDK for Embedded C.
 * It is licensed under the BSD 3-Clause license; you may not use this file
 * except in compliance with the License.
 *
 * You may obtain a copy of the License at:
 *  https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iotc_bsp_time.h>

#include <hw_types.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include "iotc_bsp_time_cc3220sf_sntp.h"

Clock_Struct clk0Struct;
void clockSecondTick( UArg arg0 );


void iotc_bsp_time_init() { 

    Clock_Params clkParams;

    Clock_Params_init( &clkParams );
    clkParams.period    = 1000; // 1 second
    clkParams.startFlag = TRUE;

    /* Construct a periodic Clock Instance */
    Clock_construct( &clk0Struct, ( Clock_FuncPtr )clockSecondTick, clkParams.period,
                     &clkParams );

    iotc_bsp_time_sntp_init( NULL );
}

iotc_time_t iotc_bsp_time_getcurrenttime_seconds() { 

    return iotc_bsp_time_sntp_getseconds_posix();
}

iotc_time_t iotc_bsp_time_getcurrenttime_milliseconds() {

    /* note this returns seconds and not milliseconds since milliseconds from EPOCH
       do not fit into 32 bits */
    return iotc_bsp_time_sntp_getseconds_posix();
}
