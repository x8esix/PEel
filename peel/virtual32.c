/*
 * (C) Copyright 2013 x8esix.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 3.0 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-3.0.txt
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include "virtual32.h"

/// <summary>
///	Changes a PE's page protections to allow for execution </summary>
///
/// <param name="vm">
/// Pointer to loaded VIRTUAL_MODULE32 </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT error </returns>
LOGICAL EXPORT LIBCALL MrProtectImage32(INOUT VIRTUAL_MODULE32* vm) {
    DWORD        dwProtect = 0;
    unsigned int i;

    if (!VirtualProtect(vm->PE.pDosHdr, vm->PE.pNtHdr->OptionalHeader.SizeOfHeaders, PAGE_READONLY, &dwProtect))
        return LOGICAL_FALSE;
    for (i = 0; i < vm->PE.pNtHdr->FileHeader.NumberOfSections; ++i) {
        dwProtect = MrSectionToPageProtection(vm->PE.ppSecHdr[i]->Characteristics);
        // virtualsize will be rounded up to page size, although we could align to SectionAlignment on our own
        if (!VirtualProtect((LPVOID)((PTR)vm->pBaseAddr + vm->PE.ppSecHdr[i]->VirtualAddress), vm->PE.ppSecHdr[i]->Misc.VirtualSize, dwProtect, &dwProtect))
            return LOGICAL_FALSE;
    }
    vm->PE.LoadStatus.Protected = TRUE;
    return LOGICAL_TRUE;
}

/// <summary>
///	Returns a PE to read & write pages for editing </summary>
///
/// <param name="vm">
/// Pointer to loaded VIRTUAL_MODULE32 </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT error </returns>
LOGICAL EXPORT LIBCALL MrUnprotectImage32(INOUT VIRTUAL_MODULE32* vm) {
    DWORD        dwProtect = 0;
    unsigned int i;

    if (!VirtualProtect(vm->PE.pDosHdr, vm->PE.pNtHdr->OptionalHeader.SizeOfHeaders, PAGE_READWRITE, &dwProtect))
        return LOGICAL_FALSE;
    for (i = 0; i < vm->PE.pNtHdr->FileHeader.NumberOfSections; ++i) {
        // virtualsize will be rounded up to page size, although we could align to SectionAlignment on our own
        if (!VirtualProtect((LPVOID)((PTR)vm->pBaseAddr + vm->PE.ppSecHdr[i]->VirtualAddress), vm->PE.ppSecHdr[i]->Misc.VirtualSize, PAGE_READWRITE, &dwProtect))
            return LOGICAL_FALSE;
    }
    vm->PE.LoadStatus.Protected = FALSE;
    return LOGICAL_TRUE;
}

/// <summary>
///	Fills VIRTUAL_MODULE32 with loaded image's information </summary>
///
/// <param name="pModuleBase">
/// Base address of target image </param>
/// <param name="vm">
/// Pointer to VIRTUAL_MODULE32 struct to recieve information about target </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT error </returns>
LOGICAL EXPORT LIBCALL MrAttachImage32(IN const void* const pModuleBase, OUT VIRTUAL_MODULE32* vm) {
    unsigned int i;
    
    // leave other members alone (name needs to be set externally)
    memset(&vm->PE, 0, sizeof(RAW_PE32));

    vm->pBaseAddr = (void*)pModuleBase;
    vm->PE.pDosHdr = (DOS_HEADER*)pModuleBase;
#if ! ACCEPT_INVALID_SIGNATURES
    if (vm->PE.pDosHdr->e_magic != IMAGE_DOS_SIGNATURE)
        return LOGICAL_FALSE;
#endif
    vm->PE.pDosStub = (DOS_STUB*)((PTR)vm->pBaseAddr + sizeof(DOS_HEADER));
    vm->PE.pNtHdr = (NT_HEADERS32*)((PTR)vm->pBaseAddr + vm->PE.pDosHdr->e_lfanew);
#if ! ACCEPT_INVALID_SIGNATURES
    if (vm->PE.pNtHdr->Signature != IMAGE_NT_SIGNATURE
     || vm->PE.pNtHdr->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        dmsg(TEXT("\nNT Headers signature or magic invalid!"));
        return LOGICAL_FALSE;
    }
#endif
    if (vm->PE.pNtHdr->FileHeader.NumberOfSections) {
        vm->PE.ppSecHdr = (SECTION_HEADER**)malloc(vm->PE.pNtHdr->FileHeader.NumberOfSections * sizeof(SECTION_HEADER*));
        if (vm->PE.ppSecHdr == NULL)
            return LOGICAL_MAYBE;
        vm->PE.ppSectionData = (void**)malloc(vm->PE.pNtHdr->FileHeader.NumberOfSections * sizeof(void*));
        if (vm->PE.ppSectionData == NULL)
            return LOGICAL_MAYBE;
        for (i = 0; i < vm->PE.pNtHdr->FileHeader.NumberOfSections; ++i) {
            vm->PE.ppSecHdr[i] = (SECTION_HEADER*)((PTR)&vm->PE.pNtHdr->OptionalHeader + vm->PE.pNtHdr->FileHeader.SizeOfOptionalHeader + (sizeof(SECTION_HEADER) * i));
            vm->PE.ppSectionData[i] = (void*)((PTR)vm->pBaseAddr + vm->PE.ppSecHdr[i]->VirtualAddress);
        }
    } else {
        vm->PE.ppSecHdr = NULL;
        vm->PE.ppSectionData = NULL;
        dmsg(TEXT("\nPE image at 0x%p has 0 sections!"), vm->pBaseAddr);
    }
    memset(&vm->PE.LoadStatus, 0, sizeof(vm->PE.LoadStatus));
    vm->PE.LoadStatus.Attached = TRUE;
    dmsg(TEXT("\nAttached to PE image at 0x%p"), vm->pBaseAddr);
    return LOGICAL_TRUE;
}

/// <summary>
///	Releases memory allocated by XxAttachImage32 </summary>
///
/// <param name="vm">
/// Pointer to loaded VIRTUAL_MODULE32 struct </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT error </returns>
LOGICAL EXPORT LIBCALL MrDetachImage32(INOUT VIRTUAL_MODULE32* vm) {
    VIRTUAL_MODULE32 *vmNext,
                     *vmPrev;
    
    if (!vm->PE.LoadStatus.Attached)
        return LOGICAL_FALSE;

    if (vm->PE.ppSecHdr != NULL)
        free(vm->PE.ppSecHdr);
    if (vm->PE.ppSectionData != NULL)
        free(vm->PE.ppSectionData);
    dmsg(TEXT("\nUnlinking PE Image at %p"), vm->pBaseAddr);
    vmPrev = (VIRTUAL_MODULE32*)vm->Blink;
    vmNext = (VIRTUAL_MODULE32*)vm->Flink;
    if (vmPrev != NULL)
        vmPrev->Flink = vmNext;
    if (vmNext != NULL)
        vmNext->Blink = vmPrev;
    dmsg(TEXT("\nDetached from PE image at 0x%p"), vm->pBaseAddr);
    memset(vm, 0, sizeof(VIRTUAL_MODULE32));
    return LOGICAL_TRUE;
}

/// <summary>
///	Converts image to file alignment </summary>
///
/// <param name="vpe">
/// Pointer to VIRTUAL_MODULE32 containing loaded image </param>
/// <param name="rpe">
/// Pointer to RAW_PE32 struct </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT/memory error </returns>
LOGICAL EXPORT LIBCALL MrImageToFile32(IN const VIRTUAL_MODULE32* vm, OUT RAW_PE32* rpe) {
    PTR32 MaxPa;
    void* pImage = NULL;

    if (!LOGICAL_SUCCESS(MrMaxPa32(&vm->PE, &MaxPa)))
        return LOGICAL_FALSE;
    pImage = VirtualAlloc(NULL, MaxPa, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (pImage == NULL)
        return LOGICAL_MAYBE;
    return MrImageToFile32Ex(vm, pImage, rpe);
}

/// <summary>
///	Converts image to file alignment </summary>
///
/// <param name="vpe">
/// Pointer to VIRTUAL_MODULE32 containing loaded image </param>
/// <param name="pImageBuffer">
/// Buffer of at least MrMaxPa32(&vm->Pe,) size with at least PAGE_READWRITE attributes
/// <param name="rpe">
/// Pointer to RAW_PE32 struct </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT/memory error </returns>
LOGICAL EXPORT LIBCALL MrImageToFile32Ex(IN const VIRTUAL_MODULE32* vm, IN const void* pBuffer, OUT RAW_PE32* rpe) {
    PTR32 MaxPa;
    unsigned int i;
    
    // unnecessary per standard, but let's play nice with gaps
    if (!LOGICAL_SUCCESS(MrMaxPa32(&vm->PE, &MaxPa)))
        return LOGICAL_FALSE;
    memset((void*)pBuffer, 0, MaxPa);
    
    rpe->pDosHdr = (DOS_HEADER*)pBuffer;
    memmove(rpe->pDosHdr, vm->PE.pDosHdr, sizeof(DOS_HEADER));
    rpe->pDosStub = (DOS_STUB*)((PTR)rpe->pDosHdr + sizeof(DOS_HEADER));
    memmove(rpe->pDosStub, vm->PE.pDosStub, rpe->pDosHdr->e_lfanew - sizeof(DOS_HEADER));
    rpe->pNtHdr = (NT_HEADERS32*)((PTR)rpe->pDosHdr + rpe->pDosHdr->e_lfanew);
    memmove(rpe->pNtHdr, vm->PE.pNtHdr, sizeof(NT_HEADERS32));
    if (rpe->pNtHdr->FileHeader.NumberOfSections) {
        rpe->ppSecHdr = (SECTION_HEADER**)malloc(rpe->pNtHdr->FileHeader.NumberOfSections * sizeof(SECTION_HEADER*));
        if (rpe->ppSecHdr == NULL)
            return LOGICAL_MAYBE;
        rpe->ppSectionData = (void**)malloc(rpe->pNtHdr->FileHeader.NumberOfSections * sizeof(void*));
        if (rpe->ppSectionData == NULL)
            return LOGICAL_MAYBE;
        for (i = 0; i < rpe->pNtHdr->FileHeader.NumberOfSections; ++i) {
            rpe->ppSecHdr[i] = (SECTION_HEADER*)((PTR)&rpe->pNtHdr->OptionalHeader + rpe->pNtHdr->FileHeader.SizeOfOptionalHeader + sizeof(SECTION_HEADER) * i);
            memmove(rpe->ppSecHdr[i], vm->PE.ppSecHdr[i], sizeof(SECTION_HEADER));
            rpe->ppSectionData[i] = (void*)((PTR)rpe->pDosHdr + rpe->ppSecHdr[i]->PointerToRawData);
            memmove(rpe->ppSectionData[i], vm->PE.ppSectionData[i], rpe->ppSecHdr[i]->SizeOfRawData);
        }
    } else {
        rpe->ppSecHdr = NULL;
        rpe->ppSectionData = NULL;
    }
    memset(&rpe->LoadStatus, 0, sizeof(rpe->LoadStatus));
    rpe->LoadStatus = vm->PE.LoadStatus;
    rpe->LoadStatus.Attached = FALSE;
    return LOGICAL_TRUE;
}

/// <summary>
///	Copies an image and fills in cvm with new information, also linked list is
/// adjusted and copied module is inserted after original </summary>
///
/// <param name="rpe">
/// Pointer to VIRTUAL_MODULE32 containing image </param>
/// <param name="crpe">
/// Pointer to VIRTUAL_MODULE32 that will recieve copy info </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT/memory error </returns>
LOGICAL EXPORT LIBCALL MrCopyImage32(IN VIRTUAL_MODULE32* vm, OUT VIRTUAL_MODULE32* cvm) {
    PTR32 MaxPa;
    void* pCopy = NULL;

    if (!LOGICAL_SUCCESS(MrMaxRva32(&vm->PE, &MaxPa)))
        return LOGICAL_FALSE;
    pCopy = VirtualAlloc(NULL, MaxPa, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (pCopy == NULL)
        return LOGICAL_MAYBE;
    return MrCopyImage32Ex(vm, (void*)pCopy, cvm);
}

/// <summary>
///	Copies an image into provided buffer and fills in cvm with new information, also linked list is
/// adjusted and copied module is inserted after original </summary>
///
/// <param name="rpe">
/// Pointer to VIRTUAL_MODULE32 containing image </param>
/// <param name="pBuffer">
/// Pointer to a buffer of at least MrMaxRva32(rpe,) bytes with at least PAGE_READWRITE access
/// <param name="crpe">
/// Pointer to VIRTUAL_MODULE32 that will recieve copy info </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT/memory error </returns>
LOGICAL EXPORT LIBCALL MrCopyImage32Ex(IN VIRTUAL_MODULE32* vm, IN const void* pBuffer, OUT VIRTUAL_MODULE32* cvm) {
    PTR32 MaxPa = 0;
    unsigned int i;

    // unnecessary per standard, but let's play nice with gaps
    if (!LOGICAL_SUCCESS(MrMaxRva32(&vm->PE, &MaxPa)))
        return LOGICAL_FALSE;
    memset((void*)pBuffer, 0, MaxPa);

    cvm->pBaseAddr = (void*)pBuffer;
    cvm->PE.pDosHdr = (DOS_HEADER*)cvm->pBaseAddr;
    memmove(cvm->PE.pDosHdr, vm->pBaseAddr, sizeof(DOS_HEADER));
    cvm->PE.pDosStub = (DOS_STUB*)((PTR)cvm->pBaseAddr + sizeof(DOS_HEADER));
    memmove(cvm->PE.pDosStub, vm->PE.pDosStub, (PTR)cvm->PE.pDosHdr->e_lfanew - sizeof(DOS_HEADER));
    cvm->PE.pNtHdr = (NT_HEADERS32*)((PTR)cvm->pBaseAddr + cvm->PE.pDosHdr->e_lfanew);
    memmove(cvm->PE.pNtHdr, vm->PE.pNtHdr, sizeof(NT_HEADERS32));
    if (cvm->PE.pNtHdr->FileHeader.NumberOfSections) {
        cvm->PE.ppSecHdr = (SECTION_HEADER**)malloc(cvm->PE.pNtHdr->FileHeader.NumberOfSections * sizeof(SECTION_HEADER*));
        if (cvm->PE.ppSecHdr == NULL)
            return LOGICAL_MAYBE;
        cvm->PE.ppSectionData = (void**)malloc(cvm->PE.pNtHdr->FileHeader.NumberOfSections * sizeof(void*));
        if (cvm->PE.ppSectionData == NULL)
            return LOGICAL_MAYBE;
        for (i = 0; i < cvm->PE.pNtHdr->FileHeader.NumberOfSections; ++i) {
            cvm->PE.ppSecHdr[i] = (SECTION_HEADER*)((PTR)&cvm->PE.pNtHdr->OptionalHeader + cvm->PE.pNtHdr->FileHeader.SizeOfOptionalHeader + sizeof(SECTION_HEADER) * i);
            memmove(cvm->PE.ppSecHdr[i], vm->PE.ppSecHdr[i], sizeof(SECTION_HEADER));
            cvm->PE.ppSectionData[i] = (void*)((PTR)cvm->pBaseAddr + cvm->PE.ppSecHdr[i]->VirtualAddress);
            memmove(cvm->PE.ppSectionData[i], vm->PE.ppSectionData[i], cvm->PE.ppSecHdr[i]->Misc.VirtualSize);
        }
    } else {
        cvm->PE.ppSecHdr = NULL;
        cvm->PE.ppSectionData = NULL;
    }
    memset(&cvm->PE.LoadStatus, 0, sizeof(PE_FLAGS));
    cvm->PE.LoadStatus = vm->PE.LoadStatus;
    cvm->PE.LoadStatus.Attached = FALSE;
    cvm->Blink = (void*)vm;
    cvm->Flink = vm->Flink;
    vm->Flink = (void*)cvm;
    return LOGICAL_TRUE;
}

/// <summary>
///	Frees a mapped image that was allocated </summary>
///
/// <param name="vm">
/// Loaded VIRTUAL_MODULE32 struct that is not attached </param>
///
/// <returns>
/// LOGICAL_TRUE on success, LOGICAL_FALSE on PE related error, LOGICAL_MAYBE on CRT/memory error, *vm is zeroed </returns>
LOGICAL EXPORT LIBCALL MrFreeImage32(INOUT VIRTUAL_MODULE32* vm) {
    VIRTUAL_MODULE32 *vmNext,
                     *vmPrev;
    
    if (vm->PE.LoadStatus.Attached == TRUE)
        return LOGICAL_FALSE;

    if (vm->PE.ppSecHdr != NULL)
        free(vm->PE.ppSecHdr);
    if (vm->PE.ppSectionData != NULL)
        free(vm->PE.ppSectionData);
    VirtualFree(vm->pBaseAddr, 0, MEM_RELEASE);

    dmsg(TEXT("\nUnlinking PE Image at %p"), vm->pBaseAddr);
    vmPrev = (VIRTUAL_MODULE32*)vm->Blink;
    vmNext = (VIRTUAL_MODULE32*)vm->Flink;
    if (vmPrev != NULL)
        vmPrev->Flink = vmNext;
    if (vmNext != NULL)
        vmNext->Blink = vmPrev;
    memset(vm, 0, sizeof(VIRTUAL_MODULE32));
    return LOGICAL_TRUE;
}