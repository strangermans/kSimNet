/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 *         Marco Miozzo <marco.miozzo@cttc.es>
 */

#ifndef FF_MAC_SCHEDULER_H
#define FF_MAC_SCHEDULER_H

#include <ns3/object.h>


namespace ns3 {



class NrFfMacCschedSapUser;
class NrFfMacSchedSapUser;
class NrFfMacCschedSapProvider;
class NrFfMacSchedSapProvider;
class NrFfrSapProvider;
class NrFfrSapUser;

/**
 * \ingroup nr
 * \defgroup ff-api FF MAC Schedulers
 */
     
/**
 * \ingroup ff-api
 *
 * This abstract base class identifies the interface by means of which
 * the helper object can plug on the MAC a scheduler implementation based on the
 * FF MAC Sched API.
 *
 *
 */
class NrFfMacScheduler : public Object
{
public:
  /**
  * The type of UL CQI to be filtered (ALL means accept all the CQI,
  * where a new CQI of any type overwrite the old one, even of another type)
  *
  */
  enum UlCqiFilter_t
  {
    SRS_UL_CQI,
    PUSCH_UL_CQI,
    ALL_UL_CQI
  };
  /**
  * constructor
  *
  */
  NrFfMacScheduler ();
  /**
   * destructor
   *
   */
  virtual ~NrFfMacScheduler ();

  // inherited from Object
  virtual void DoDispose (void);
  static TypeId GetTypeId (void);

  /**
   * set the user part of the FfMacCschedSap that this Scheduler will
   * interact with. Normally this part of the SAP is exported by the MAC.
   *
   * \param s
   */
  virtual void SetNrFfMacCschedSapUser (NrFfMacCschedSapUser* s) = 0;

  /**
   *
   * set the user part of the FfMacSchedSap that this Scheduler will
   * interact with. Normally this part of the SAP is exported by the MAC.
   *
   * \param s
   */
  virtual void SetNrFfMacSchedSapUser (NrFfMacSchedSapUser* s) = 0;

  /**
   *
   * \return the Provider part of the FfMacCschedSap provided by the Scheduler
   */
  virtual NrFfMacCschedSapProvider* GetNrFfMacCschedSapProvider () = 0;

  /**
   *
   * \return the Provider part of the FfMacSchedSap provided by the Scheduler
   */
  virtual NrFfMacSchedSapProvider* GetNrFfMacSchedSapProvider () = 0;

  //FFR SAPs
  /**
   *
   * Set the Provider part of the NrFfrSap that this Scheduler will
   * interact with
   *
   * \param s
   */
  virtual void SetNrFfrSapProvider (NrFfrSapProvider* s) = 0;

  /**
   *
   * \return the User part of the NrFfrSap provided by the FfrAlgorithm
   */
  virtual NrFfrSapUser* GetNrFfrSapUser () = 0;
  
protected:
    
  UlCqiFilter_t m_ulCqiFilter;

};

}  // namespace ns3

#endif /* FF_MAC_SCHEDULER_H */
