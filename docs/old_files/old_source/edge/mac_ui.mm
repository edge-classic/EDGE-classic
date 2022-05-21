//----------------------------------------------------------------------------
//  EDGE MAC UI System Code (Error messages etc...)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
#import <Cocoa/Cocoa.h>

//
// I_MessageBox
//
// Generate a message box displaying a notice and 'OK' button.
//
void I_MessageBox(const char *_msg, const char *_title)
{
    // Get the system default encoding
    const NSStringEncoding default_encoding = [NSString defaultCStringEncoding];

	// Convert strings to NSStrings for use with Cocoa
	NSString *title = [[NSString alloc] initWithCString:_title 
                                        encoding:default_encoding];

	NSString *msg = [[NSString alloc] initWithCString:_msg 
                                      encoding:default_encoding];

	// Generate panel and await user feedback
	NSRunAlertPanel(title, msg, @"OK", nil, nil);

	// Release strings when finished
	[msg release];
	[title release];
}
