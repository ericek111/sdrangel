///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "samplemififo.h"

SampleMIFifo::SampleMIFifo(QObject *parent) :
    QObject(parent)
{
}

SampleMIFifo::SampleMIFifo(unsigned int nbStreams, unsigned int size, QObject *parent) :
    QObject(parent)
{
    for (unsigned int i = 0; i < nbStreams; i++)
    {
        m_data.push_back(SampleVector());
        m_data.back().resize(size);
        m_vFill.push_back(0);
        m_vHead.push_back(0);
    }
}

void SampleMIFifo::init(unsigned int nbStreams, unsigned int size)
{
    m_data.clear();
    m_vFill.clear();
    m_vHead.clear();

    for (unsigned int i = 0; i < nbStreams; i++)
    {
        m_data.push_back(SampleVector());
        m_data.back().resize(size);
        m_vFill.push_back(0);
        m_vHead.push_back(0);
    }
}

void SampleMIFifo::writeSync(const std::vector<SampleVector::const_iterator>& vbegin, unsigned int size)
{
    if ((m_data.size() == 0) || (m_data.size() != vbegin.size())) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);

    for (unsigned int stream = 0; stream < m_data.size(); stream++)
    {
        int spaceLeft = m_data[stream].size() - m_vFill[stream];

        if (size < spaceLeft)
        {
            std::copy(vbegin[stream], vbegin[stream] + size, m_data[stream].begin() + m_vFill[stream]);
            m_vFill[stream] += size;
        }
        else
        {
            int remaining = size - spaceLeft;
            std::copy(vbegin[stream], vbegin[stream] + spaceLeft, m_data[stream].begin() + m_vFill[stream]);
            std::copy(vbegin[stream] + spaceLeft, vbegin[stream] + size, m_data[stream].begin());
            m_vFill[stream] = remaining;
        }
    }

    emit dataSyncReady();
}

void SampleMIFifo::writeAsync(unsigned int stream, const SampleVector::const_iterator& begin, unsigned int size)
{
    if (stream < m_data.size())
    {
        QMutexLocker mutexLocker(&m_mutex);
        int spaceLeft = m_data[stream].size() - m_vFill[stream];

        if (size < spaceLeft)
        {
            std::copy(begin, begin + size, m_data[stream].begin() + m_vFill[stream]);
            m_vFill[stream] += size;
        }
        else
        {
            int remaining = size - spaceLeft;
            std::copy(begin, begin + spaceLeft, m_data[stream].begin() + m_vFill[stream]);
            std::copy(begin + spaceLeft, begin + size,  m_data[stream].begin());
            m_vFill[stream] = remaining;
        }

        emit dataAsyncReady(stream);
    }
}

void SampleMIFifo::readSync(
        std::vector<SampleVector::const_iterator>& vpart1Begin, std::vector<SampleVector::const_iterator>& vpart1End,
        std::vector<SampleVector::const_iterator>& vpart2Begin, std::vector<SampleVector::const_iterator>& vpart2End
)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::vector<SampleVector>::iterator dataIt = m_data.begin();
    int stream = 0;

    for (; dataIt != m_data.end(); ++dataIt, ++stream)
    {
        if (m_vHead[stream] < m_vFill[stream])
        {
            vpart1Begin.push_back(dataIt->begin() + m_vHead[stream]);
            vpart1End.push_back(dataIt->begin() + m_vFill[stream]);
            vpart2Begin.push_back(dataIt->begin());
            vpart2End.push_back(dataIt->begin());
        }
        else
        {
            vpart1Begin.push_back(dataIt->begin() + m_vHead[stream]);
            vpart1End.push_back(dataIt->end());
            vpart2Begin.push_back(dataIt->begin());
            vpart2End.push_back(dataIt->begin() + m_vFill[stream]);
        }

        m_vHead[stream] = m_vFill[stream];
    }
}

void SampleMIFifo::readSync(
        std::vector<int>& vPart1Begin, std::vector<int>& vPart1End,
        std::vector<int>& vPart2Begin, std::vector<int>& vPart2End
)
{
    if (m_data.size() == 0) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    std::vector<SampleVector>::iterator dataIt = m_data.begin();
    int stream = 0;

    for (; dataIt != m_data.end(); ++dataIt, ++stream)
    {
        if (m_vHead[stream] < m_vFill[stream])
        {
            vPart1Begin.push_back(m_vHead[stream]);
            vPart1End.push_back(m_vFill[stream]);
            vPart2Begin.push_back(0);
            vPart2End.push_back(0);
        }
        else
        {
            vPart1Begin.push_back(m_vHead[stream]);
            vPart1End.push_back(dataIt->size());
            vPart2Begin.push_back(0);
            vPart2End.push_back(m_vFill[stream]);
        }

        m_vHead[stream] = m_vFill[stream];
    }
}

void SampleMIFifo::readAsync(unsigned int stream,
		SampleVector::const_iterator& part1Begin, SampleVector::const_iterator& part1End,
		SampleVector::const_iterator& part2Begin, SampleVector::const_iterator& part2End)
{
    if (stream < m_data.size())
    {
        QMutexLocker mutexLocker(&m_mutex);

        if (m_vHead[stream] < m_vFill[stream])
        {
            part1Begin = m_data[stream].begin() + m_vHead[stream];
            part1End   = m_data[stream].begin() + m_vFill[stream];
            part2Begin = m_data[stream].begin();
            part2End   = m_data[stream].begin();
        }
        else
        {
            part1Begin = m_data[stream].begin() + m_vHead[stream];
            part1End   = m_data[stream].end();
            part2Begin = m_data[stream].begin();
            part2End   = m_data[stream].begin() + m_vFill[stream];
        }

        m_vHead[stream] = m_vFill[stream];
    }
}

bool SampleMIFifo::dataAvailable()
{
    if (m_data.size() == 0) {
        return false;
    }

    QMutexLocker mutexLocker(&m_mutex);
    bool value;
    value = m_vHead[0] != m_vFill[0];

    return value;
}

bool SampleMIFifo::dataAvailable(unsigned int stream)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool value;

    if (stream < m_data.size()) {
        value = m_vHead[stream] != m_vFill[stream];
    } else {
        value = false;
    }

    return value;
}
