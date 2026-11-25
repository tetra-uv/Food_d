#ifndef PAGEDONOREDIT_H
#define PAGEDONOREDIT_H

// PageDonorEdit.h: the hidden Edit donation page of a donor, reached only from My donations.

#include "GuiPage.h"

// Makes the Edit donation page (the shell owns it like every other page).
guipage* newpagedonoredit();

// Shows the Edit donation page for the donation at this position in the data, filled with its current values.
void openeditdonation(int donationpos);

#endif
