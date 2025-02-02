/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009, 2010 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Modified by : Marco Miozzo <mmiozzo@cttc.es>
 *        (move from CQI to Ctrl and Data SINR Chunk processors)
 * Modified by : Piotr Gawlowicz <gawlowicz.p@gmail.com>
 *        (removed all Nr***ChunkProcessor implementations
 *        and created generic NrChunkProcessor)
 */


#ifndef NR_CHUNK_PROCESSOR_H
#define NR_CHUNK_PROCESSOR_H

#include <ns3/ptr.h>
#include <ns3/nstime.h>
#include <ns3/object.h>

namespace ns3 {

class SpectrumValue;

typedef Callback< void, const SpectrumValue& > NrChunkProcessorCallback;

/** 
 * This abstract class is used to process the time-vs-frequency
 * SINR/interference/power chunk of a received NR signal
 * which was calculated by the NrInterference object.
 */
class NrChunkProcessor : public SimpleRefCount<NrChunkProcessor>
{
public:
  NrChunkProcessor ();
  virtual ~NrChunkProcessor ();

  /**
    * \brief Add callback to list
    *
    * This function adds callback c to list. Each callback pass
    * calculated value to its object and is called in
    * NrChunkProcessor::End().
    */
  virtual void AddCallback (NrChunkProcessorCallback c);

  /**
    * \brief Clear internal variables
    *
    * This function clears internal variables in the beginning of
    * calculation
    */
  virtual void Start ();

  /**
    * \brief Collect SpectrumValue and duration of signal
    *
    * Passed values are collected in m_sumValues and m_totDuration variables.
    */
  virtual void EvaluateChunk (const SpectrumValue& sinr, Time duration);

  /**
    * \brief Finish calculation and inform interested objects about calculated value
    *
    * During this function all callbacks from list are executed
    * to inform interested object about calculated value. This
    * function is called at the end of calculation.
    */
  virtual void End ();

private:
  Ptr<SpectrumValue> m_sumValues;
  Time m_totDuration;

  std::vector<NrChunkProcessorCallback> m_nrChunkProcessorCallbacks;
};


/**
 * A sink to be plugged to the callback of NrChunkProcessor allowing
 * to save and later retrieve the latest reported value 
 * 
 */
class NrSpectrumValueCatcher
{
public:

  /** 
   * function to be plugged to NrChunkProcessor::AddCallback ()
   * 
   * \param value 
   */
  void ReportValue (const SpectrumValue& value);

  /** 
   * 
   * 
   * \return the latest value reported by the NrChunkProcessor
   */
  Ptr<SpectrumValue> GetValue ();
  
private:
  Ptr<SpectrumValue> m_value;
};

} // namespace ns3



#endif /* NR_CHUNK_PROCESSOR_H */
