/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Marco Miozzo  <marco.miozzo@cttc.es>
 */


#ifndef NR_HARQ_PHY_MODULE_H
#define NR_HARQ_PHY_MODULE_H



#include <ns3/log.h>
#include <ns3/assert.h>
#include <math.h>
#include <vector>
#include <map>
#include <ns3/simple-ref-count.h>


namespace ns3 {



struct HarqProcessInfoElement_t
{
   double m_mi;
   uint8_t m_rv;
   uint16_t m_infoBits;
   uint16_t m_codeBits;
};

typedef std::vector <HarqProcessInfoElement_t> HarqProcessInfoList_t;

/**
 * \ingroup nr
 * \brief The NrHarqPhy class implements the HARQ functionalities related to PHY layer
 *(i.e., decodification buffers for incremental redundancy managment)
 *
*/
class NrHarqPhy : public SimpleRefCount<NrHarqPhy>
{
public:
  NrHarqPhy ();
  ~NrHarqPhy ();

  void SubframeIndication (uint32_t frameNo, uint32_t subframeNo);

  /**
  * \brief Return the cumulated MI of the HARQ procId in case of retranmissions
  * for DL (asynchronous)
  * \param harqProcId the HARQ proc id
  * \param layer layer no. (for MIMO spatial multiplexing)
  * \return the MI accumulated
  */
  double GetAccumulatedMiDl (uint8_t harqProcId, uint8_t layer);

  /**
  * \brief Return the info of the HARQ procId in case of retranmissions
  * for DL (asynchronous)
  * \param harqProcId the HARQ proc id
  * \param layer layer no. (for MIMO spatail multiplexing)
  * \return the vector of the info related to HARQ proc Id
  */
  HarqProcessInfoList_t GetHarqProcessInfoDl (uint8_t harqProcId, uint8_t layer);

  /**
  * \brief Return the cumulated MI of the HARQ procId in case of retranmissions
  * for UL (synchronous)
  * \return the MI accumulated
  */
  double GetAccumulatedMiUl (uint16_t rnti);

  /**
  * \brief Return the info of the HARQ procId in case of retranmissions
  * for UL (asynchronous)
  * \param rnti the RNTI of the transmitter
  * \param harqProcId the HARQ proc id
  * \return the vector of the info related to HARQ proc Id
  */
  HarqProcessInfoList_t GetHarqProcessInfoUl (uint16_t rnti, uint8_t harqProcId);

  /**
  * \brief Update the Info associated to the decodification of an HARQ process
  * for DL (asynchronous)
  * \param id the HARQ proc id
  * \param layer layer no. (for MIMO spatail multiplexing)
  * \param mi the new MI
  * \param infoBytes the no. of bytes of info
  * \param mi the total no. of bytes txed
  */
  void UpdateDlHarqProcessStatus (uint8_t id, uint8_t layer, double mi, uint16_t infoBytes, uint16_t codeBytes);

  /**
  * \brief Reset  the info associated to the decodification of an HARQ process
  * for DL (asynchronous)
  * \param id the HARQ proc id
  */
  void ResetDlHarqProcessStatus(uint8_t id);

  /**
  * \brief Update the MI value associated to the decodification of an HARQ process
  * for DL (asynchronous)
  * \param rnti the RNTI of the transmitter
  * \param mi the new MI
  * \param infoBytes the no. of bytes of info
  * \param mi the total no. of bytes txed
  */
  void UpdateUlHarqProcessStatus (uint16_t rnti, double mi, uint16_t infoBytes, uint16_t codeBytes);

  /**
  * \brief Reset  the info associated to the decodification of an HARQ process
  * for DL (asynchronous)
  * \param id the HARQ proc id
  */
  void ResetUlHarqProcessStatus(uint16_t rnti, uint8_t id);
  
  
  


private:

  std::vector <std::vector <HarqProcessInfoList_t> > m_miDlHarqProcessesInfoMap;
  std::map <uint16_t, std::vector <HarqProcessInfoList_t> > m_miUlHarqProcessesInfoMap;
  

};


}

#endif /* NR_HARQ_PHY_MODULE_H */
