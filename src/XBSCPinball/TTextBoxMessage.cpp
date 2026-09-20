#include "pch.h"
#include "TTextBoxMessage.h"
#include "pb.h"

#ifdef _XBOX
// RXDK's CRT does not provide the modern MSVC __ftol2_sse helper.
// Perform the two float -> int conversions in this TU directly with x87.
// C++ float-to-int conversion truncates toward zero, so temporarily set the
// x87 rounding-control field to truncate, convert, then restore the old CW.
static int XbFloatToIntTrunc(float value)
{
	int result;
	unsigned short oldControlWord;
	unsigned short truncControlWord;

	__asm
	{
		fnstcw oldControlWord
		mov ax, oldControlWord
		and ax, 0F3FFh
		or ax, 0C00h
		mov truncControlWord, ax
		fldcw truncControlWord
		fld value
		fistp dword ptr result
		fldcw oldControlWord
	}

	return result;
}
#endif

TTextBoxMessage::TTextBoxMessage(const char* text, float time, bool lowPriority)
{
	NextMessage = nullptr;
	Time = time;
#ifdef _XBOX
	EndTicks = pb::time_ticks + XbFloatToIntTrunc(time * 1000.0f);
#else
	EndTicks = pb::time_ticks + static_cast<int>(time * 1000.0f);
#endif
	LowPriority = lowPriority;
	if (text)
	{
		const auto textLen = strlen(text) + 1;
		Text = new char[textLen];
		if (Text)
			strncpy(Text, text, textLen);
	}
	else
		Text = nullptr;
}

TTextBoxMessage::~TTextBoxMessage()
{
	delete[] Text;
}

float TTextBoxMessage::TimeLeft() const
{
	return static_cast<float>(EndTicks - pb::time_ticks) * 0.001f;
}

void TTextBoxMessage::Refresh(float time)
{
	this->Time = time;
#ifdef _XBOX
	this->EndTicks = pb::time_ticks + XbFloatToIntTrunc(time * 1000.0f);
#else
	this->EndTicks = pb::time_ticks + static_cast<int>(time * 1000.0f);
#endif
}
