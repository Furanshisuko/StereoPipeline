// __BEGIN_LICENSE__
// Copyright (C) 2006-2009 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

/// \file IsisInterfaceFrame.h
///
/// Frame Camera Interface with ISIS
///
#ifndef __ASP_ISIS_INTERFACE_FRAME_H__
#define __ASP_ISIS_INTERFACE_FRAME_H__

#include <asp/IsisIO/IsisInterface.h>

namespace asp {
namespace isis {

  class IsisInterfaceFrame : public IsisInterface {

  public:
    IsisInterfaceFrame( std::string const& filename ) {}

    virtual ~IsisInterfaceFrame() {}

    virtual std::string type()  { return "Frame"; }

  };

}}

#endif//__ASP_ISIS_INTERFACE_FRAME_H__