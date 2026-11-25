#ifndef PROMOTION_H
#define PROMOTION_H

// ProMotion.h: the only motion of the professional program: a 120 ms fade on a page switch and a 120 ms hover change of the
// buttons (no count up, decision ICON POLICY AND CALM PASS). One timer of the main window runs only while something moves.
// Nothing moves when the Windows setting "show animations" is off, or in a test mode.

#include <windows.h>

// The id of the motion timer of the main window.
static const UINT_PTR motiontimer = 43;

// True when motion is wanted (the system animation setting is on and no test mode runs).
bool promotionon();

// Fades the page host in from the page tone (call after a page switch).
void promotionfade(HWND main, HWND host);

// Starts the 120 ms hover change of a button.
void promotionhover(HWND button);

// How far the hover change of a button is, 0 to 100 (100 when none runs).
int promotionhoverstep(HWND button);

// Handles the motion timer of the main window; returns true when the timer was the motion timer.
bool promotiontick(HWND main, UINT_PTR timer);

#endif
