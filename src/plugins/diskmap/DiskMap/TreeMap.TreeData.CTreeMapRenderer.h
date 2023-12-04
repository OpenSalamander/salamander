// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.TreeData.h"
#include "TreeMap.TreeData.CCushionRow.h"

class CTreeMapRendererMaxRatio : public CTreeMapRendererBase
{
protected:
    double _maxratio;

public:
    CTreeMapRendererMaxRatio(EDirectionStyle dirstyle, double maxratio) : CTreeMapRendererBase(dirstyle)
    {
        _maxratio = maxratio;
    }
    ERenderDecision Decide(CCushionRow* csr, double width, double length, INT64 datasize, INT64 remainingdata)
    {
        //double lastRatio = 1.0E100;
        //int currentSum = 0;
        if (csr == NULL)
            return rdNewRow;
        //if (csr->IsEmpty() == 0) return rdSameRow;

        CCushion* cs = csr->GetLastCushion();
        if (cs == NULL)
            return rdSameRow;

        INT64 lastSize = cs->GetDataSize();

        double lastRowWidth = width / (csr->GetDataSize() + remainingdata) * (csr->GetDataSize());
        double lastBoxHeight = length / csr->GetDataSize() * lastSize;

        double newRowWidth = width / (csr->GetDataSize() + remainingdata) * (csr->GetDataSize() + datasize);
        double newBoxHeight = length / (csr->GetDataSize() + datasize) * datasize;

        double lastRatio = lastBoxHeight / lastRowWidth;
        if (lastRatio < 1)
            lastRatio = 1 / lastRatio;

        double newRatio = newBoxHeight / newRowWidth;
        if (newRatio < 1)
            newRatio = 1 / newRatio;

        if ((lastRatio >= newRatio) || (newRatio < this->_maxratio))
            return rdSameRow;
        return rdNewRow;
    }
    /*
	protected override void InnerDraw(BoxLine line, LineParams lineInfo, Data data, int remainingSize, int pos)
	{
	float rowWidth = lineInfo.MaxWidth;
	float rowLength = lineInfo.Length;

	float lastRatio = 1.0E30f;
	int currentSum = 0;

	int firstsize = data[pos];
	for (int i = pos; i < data.Length; i++)
	{
	int currentSize = data[i];
	float newRowWidth = (rowWidth / ((float)remainingSize)) * (currentSum + currentSize);
	float newBoxHeight = (rowLength / ((float)(currentSum + currentSize))) * currentSize;
	//float num11 = (height / ((float)(currentSum + currentSize))) * currentSize;

	//num11 = (height / ((float)(currentSum + currentSize))) * firstsize;
	//float num12 = Math.Max((float)(num11 / num9), (float)(num9 / num11));

	float newRatio = Math.Max((float)(newBoxHeight / newRowWidth), (float)(newRowWidth / newBoxHeight));
	if ((lastRatio >= newRatio) || (newRatio < this._ratio))
	{
	line.AddBox(currentSize, i);
	currentSum += currentSize;
	}
	else
	{
	break;
	}
	lastRatio = newRatio;
	}
	}
	*/
};
