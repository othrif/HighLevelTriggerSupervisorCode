// this is -*- c++ -*-
#ifndef __HLTSV_ISSUES_H__
#define __HLTSV_ISSUES_H__

#include "ers/Issue.h"

#ifndef ERS_EMPTY
#define ERS_EMPTY /* */
#endif

ERS_DECLARE_ISSUE( 	hltsv, // namespace
			Issue, // issue class name
			" HLT Supervisor Issue "
                        ERS_EMPTY,
			ERS_EMPTY
                 )

ERS_DECLARE_ISSUE_BASE(	hltsv, // namespace
			FilarDevException, // issue class name
			hltsv::Issue, // base class name
			" Filar rcc errcode "
			<< std::hex << rcc_errcode 
			<< std::dec << "\".",         // message
                        ERS_EMPTY,
                        ((unsigned int)rcc_errcode )  // attribute of this class
			)

ERS_DECLARE_ISSUE_BASE(	hltsv, // namespace
			NoL1ResultsException, // issue class name
			hltsv::Issue, // base class name
			" No LVL1 Results in specified file(s) "
			<< "\".",                    // message
                        ERS_EMPTY,
                        ERS_EMPTY
			)

ERS_DECLARE_ISSUE_BASE( hltsv, // namespace
			FilarFailed, // issue class name
			hltsv::Issue, // base class name
			" Access to Filar failed on "<<
			what << "\".", //message
                        ERS_EMPTY,
			((const char *)what)
			)

ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			FileFailed, // issue class name
			hltsv::Issue, //base class name
			" Preloaded file problem while "<<
			what <<"\".", //message
                        ERS_EMPTY,
			((const char *)what)  //attribute
			)

ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			SlinkFailed, // issue class name
			hltsv::Issue, //base class name
			" Caught SLink Exception", //message
                        ERS_EMPTY,
                        ERS_EMPTY
			)

ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			ErrorRecovery, // issue class name
			hltsv::Issue, //base class name
			" Error Recovery"<<
			what<<"\".", //message
                        ERS_EMPTY,
			((const char*) what )) //attribute of this class

ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			InitialisationFailure, // issue class name
			hltsv::Issue, //base class name
			" Initialisation failed, "<<
			what<<"\".", //message
                        ERS_EMPTY,
			((const char*) what )) //attribute of this class

ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			ConfigFailed, // issue class name
			hltsv::Issue, //base class name
			" Configuration failed "<<
			what<<"\".", //message
                        ERS_EMPTY,
			((const char*) what )) //attribute of this class


ERS_DECLARE_ISSUE_BASE( hltsv, //namespace
			EventFailed, // issue class name
			hltsv::Issue, //base class name
			" Event Processing problem "<<
			what << "\".", //message
                        ERS_EMPTY,
			((const char*) what )) //attribute of this class

#endif //__HLTSV_ISSUES_H__
