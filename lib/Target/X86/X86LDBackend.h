//===- X86LDBackend.h -----------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef X86_LDBACKEND_H
#define X86_LDBACKEND_H

#include "X86ELFDynamic.h"
#include "X86GOT.h"
#include "X86GOTPLT.h"
#include "X86PLT.h"
#include <mcld/LD/LDSection.h>
#include <mcld/Target/GNULDBackend.h>
#include <mcld/Target/OutputRelocSection.h>

namespace mcld {

class LinkerConfig;
class GNUInfo;

//===----------------------------------------------------------------------===//
/// X86GNULDBackend - linker backend of X86 target of GNU ELF format
///
class X86GNULDBackend : public GNULDBackend
{
public:
  /** \enum ReservedEntryType
   *  \brief The reserved entry type of reserved space in ResolveInfo.
   *
   *  This is used for sacnRelocation to record what kinds of entries are
   *  reserved for this resolved symbol
   *
   *  In X86, there are three kinds of entries, GOT, PLT, and dynamic reloction.
   *  GOT may needs a corresponding relocation to relocate itself, so we
   *  separate GOT to two situations: GOT and GOTRel. Besides, for the same
   *  symbol, there might be two kinds of entries reserved for different location.
   *  For example, reference to the same symbol, one may use GOT and the other may
   *  use dynamic relocation.
   *
   *  bit:  3       2      1     0
   *   | PLT | GOTRel | GOT | Rel |
   *
   *  value    Name         - Description
   *
   *  0000     None         - no reserved entry
   *  0001     ReserveRel   - reserve an dynamic relocation entry
   *  0010     ReserveGOT   - reserve an GOT entry
   *  0011     GOTandRel    - For different relocation, we've reserved GOT and
   *                          Rel for different location.
   *  0100     GOTRel       - reserve an GOT entry and the corresponding Dyncamic
   *                          relocation entry which relocate this GOT entry
   *  0101     GOTRelandRel - For different relocation, we've reserved GOTRel
   *                          and relocation entry for different location.
   *  1000     ReservePLT   - reserve an PLT entry and the corresponding GOT,
   *                          Dynamic relocation entries
   *  1001     PLTandRel    - For different relocation, we've reserved PLT and
   *                          Rel for different location.
   */
  enum ReservedEntryType {
    None         = 0,
    ReserveRel   = 1,
    ReserveGOT   = 2,
    GOTandRel    = 3,
    GOTRel       = 4,
    GOTRelandRel = 5,
    ReservePLT   = 8,
    PLTandRel    = 9
  };

public:
  X86GNULDBackend(const LinkerConfig& pConfig,
		  GNUInfo* pInfo,
		  Relocation::Type pCopyRel);

  ~X86GNULDBackend();

  uint32_t machine() const;

  X86PLT& getPLT();

  const X86PLT& getPLT() const;

  /// preLayout - Backend can do any needed modification before layout
  void doPreLayout(IRBuilder& pBuilder);

  /// postLayout -Backend can do any needed modification after layout
  void doPostLayout(Module& pModule, IRBuilder& pBuilder);

  /// dynamic - the dynamic section of the target machine.
  /// Use co-variant return type to return its own dynamic section.
  X86ELFDynamic& dynamic();

  /// dynamic - the dynamic section of the target machine.
  /// Use co-variant return type to return its own dynamic section.
  const X86ELFDynamic& dynamic() const;

  /// emitSectionData - write out the section data into the memory region.
  /// When writers get a LDSection whose kind is LDFileFormat::Target, writers
  /// call back target backend to emit the data.
  ///
  /// Backends handle the target-special tables (plt, gp,...) by themselves.
  /// Backend can put the data of the tables in MCSectionData directly
  ///  - LDSection.getSectionData can get the section data.
  /// Or, backend can put the data into special data structure
  ///  - backend can maintain its own map<LDSection, table> to get the table
  /// from given LDSection.
  ///
  /// @param pSection - the given LDSection
  /// @param pLayout - for comouting the size of fragment
  /// @param pRegion - the region to write out data
  /// @return the size of the table in the file.
  uint64_t emitSectionData(const LDSection& pSection,
                           MemoryRegion& pRegion) const;

  /// initRelocator - create and initialize Relocator.
  virtual bool initRelocator() = 0;

  /// getRelocator - return relocator.
  Relocator* getRelocator();

  virtual void initTargetSections(Module& pModule, ObjectBuilder& pBuilder) = 0;

  void initTargetSymbols(IRBuilder& pBuilder, Module& pModule);

  /// scanRelocation - determine the empty entries are needed or not and create
  /// the empty entries if needed.
  /// For X86, following entries are check to create:
  /// - GOT entry (for .got and .got.plt sections)
  /// - PLT entry (for .plt section)
  /// - dynamin relocation entries (for .rel.plt and .rel.dyn sections)
  void scanRelocation(Relocation& pReloc,
                      IRBuilder& pBuilder,
                      Module& pModule,
                      LDSection& pSection);

  OutputRelocSection& getRelDyn();

  const OutputRelocSection& getRelDyn() const;

  OutputRelocSection& getRelPLT();

  const OutputRelocSection& getRelPLT() const;

  /// getTargetSectionOrder - compute the layout order of X86 target sections
  unsigned int getTargetSectionOrder(const LDSection& pSectHdr) const;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols();

  /// getPointerRel - get pointer relocation type. 
  Relocation::Type getPointerRel()
  { return m_PointerRel; }

private:
  virtual void scanLocalReloc(Relocation& pReloc,
			      IRBuilder& pBuilder,
			      Module& pModule,
			      LDSection& pSection) = 0;

  virtual void scanGlobalReloc(Relocation& pReloc,
			       IRBuilder& pBuilder,
			       Module& pModule,
			       LDSection& pSection) = 0;

protected:
  /// addCopyReloc - add a copy relocation into .rel.dyn for pSym
  /// @param pSym - A resolved copy symbol that defined in BSS section
  void addCopyReloc(ResolveInfo& pSym);

  /// defineSymbolforCopyReloc - allocate a space in BSS section and
  /// and force define the copy of pSym to BSS section
  /// @return the output LDSymbol of the copy symbol
  LDSymbol& defineSymbolforCopyReloc(IRBuilder& pLinker,
                                     const ResolveInfo& pSym);

  void defineGOTSymbol(IRBuilder& pBuilder, Fragment&);

protected:
  /// getRelEntrySize - the size in BYTE of rel type relocation
  size_t getRelEntrySize()
  { return m_RelEntrySize; }

  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize()
  { return m_RelaEntrySize; }

private:
  /// doCreateProgramHdrs - backend can implement this function to create the
  /// target-dependent segments
  void doCreateProgramHdrs(Module& pModule);

  virtual void setGOTSectionSize(IRBuilder& pBuilder) = 0;

  virtual uint64_t emitGOTSectionData(MemoryRegion& pRegion) const = 0;

  virtual uint64_t emitGOTPLTSectionData(MemoryRegion& pRegion,
					 const ELFFileFormat* FileFormat) const = 0;

  virtual void setRelDynSize() = 0;
  virtual void setRelPLTSize() = 0;

protected:
  Relocator* m_pRelocator;
  X86PLT* m_pPLT;
  /// m_RelDyn - dynamic relocation table of .rel.dyn
  OutputRelocSection* m_pRelDyn;
  /// m_RelPLT - dynamic relocation table of .rel.plt
  OutputRelocSection* m_pRelPLT;

  X86ELFDynamic* m_pDynamic;
  LDSymbol* m_pGOTSymbol;

  size_t m_RelEntrySize;
  size_t m_RelaEntrySize;

  Relocation::Type m_CopyRel;
  Relocation::Type m_PointerRel;
};

//
//===----------------------------------------------------------------------===//
/// X86_32GNULDBackend - linker backend of X86-32 target of GNU ELF format
///
class X86_32GNULDBackend : public X86GNULDBackend
{
public:
  X86_32GNULDBackend(const LinkerConfig& pConfig, GNUInfo* pInfo);

  ~X86_32GNULDBackend();

  void initTargetSections(Module& pModule, ObjectBuilder& pBuilder);

  X86_32GOT& getGOT();

  const X86_32GOT& getGOT() const;

  X86_32GOTPLT& getGOTPLT();

  const X86_32GOTPLT& getGOTPLT() const;

  X86_32GOTEntry& getTLSModuleID();

private:
  void scanLocalReloc(Relocation& pReloc,
                      IRBuilder& pBuilder,
                      Module& pModule,
                      LDSection& pSection);

  void scanGlobalReloc(Relocation& pReloc,
                       IRBuilder& pBuilder,
                       Module& pModule,
                       LDSection& pSection);

  /// initRelocator - create and initialize Relocator.
  bool initRelocator();

  /// -----  tls optimization  ----- ///
  /// convert R_386_TLS_IE to R_386_TLS_LE
  void convertTLSIEtoLE(Relocation& pReloc, LDSection& pSection);

  void setGOTSectionSize(IRBuilder& pBuilder);

  uint64_t emitGOTSectionData(MemoryRegion& pRegion) const;

  uint64_t emitGOTPLTSectionData(MemoryRegion& pRegion,
				 const ELFFileFormat* FileFormat) const;

  void setRelDynSize();
  void setRelPLTSize();

private:
  X86_32GOT* m_pGOT;
  X86_32GOTPLT* m_pGOTPLT;
};

//
//===----------------------------------------------------------------------===//
/// X86_64GNULDBackend - linker backend of X86-64 target of GNU ELF format
///
class X86_64GNULDBackend : public X86GNULDBackend
{
public:
  X86_64GNULDBackend(const LinkerConfig& pConfig, GNUInfo* pInfo);

  ~X86_64GNULDBackend();

  void initTargetSections(Module& pModule, ObjectBuilder& pBuilder);

  X86_64GOT& getGOT();

  const X86_64GOT& getGOT() const;

  X86_64GOTPLT& getGOTPLT();

  const X86_64GOTPLT& getGOTPLT() const;

private:
  void scanLocalReloc(Relocation& pReloc,
                      IRBuilder& pBuilder,
                      Module& pModule,
                      LDSection& pSection);

  void scanGlobalReloc(Relocation& pReloc,
                       IRBuilder& pBuilder,
                       Module& pModule,
                       LDSection& pSection);

  /// initRelocator - create and initialize Relocator.
  bool initRelocator();

  void setGOTSectionSize(IRBuilder& pBuilder);

  uint64_t emitGOTSectionData(MemoryRegion& pRegion) const;

  uint64_t emitGOTPLTSectionData(MemoryRegion& pRegion,
				 const ELFFileFormat* FileFormat) const;

  void setRelDynSize();
  void setRelPLTSize();

private:
  X86_64GOT* m_pGOT;
  X86_64GOTPLT* m_pGOTPLT;
};
} // namespace of mcld

#endif

