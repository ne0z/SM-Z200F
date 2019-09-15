/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKContextMenuItemTypes_h
#define WKContextMenuItemTypes_h

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKContextMenuItemTagNoAction = 0,
    kWKContextMenuItemTagOpenLinkInNewWindow,
    kWKContextMenuItemTagDownloadLinkToDisk,
    kWKContextMenuItemTagCopyLinkToClipboard,
    kWKContextMenuItemTagOpenImageInNewWindow,
#if ENABLE(TIZEN_OPEN_IMAGE_IN_CURRENT_TAB)
    kWKContextMenuItemTagOpenImageInCurrentWindow,
#endif
    kWKContextMenuItemTagDownloadImageToDisk,
    kWKContextMenuItemTagCopyImageToClipboard,
    kWKContextMenuItemTagOpenFrameInNewWindow,
    kWKContextMenuItemTagCopy,
    kWKContextMenuItemTagGoBack,
    kWKContextMenuItemTagGoForward,
    kWKContextMenuItemTagStop,
    kWKContextMenuItemTagReload,
    kWKContextMenuItemTagCut,
    kWKContextMenuItemTagPaste,
    kWKContextMenuItemTagSpellingGuess,
    kWKContextMenuItemTagNoGuessesFound,
    kWKContextMenuItemTagIgnoreSpelling,
    kWKContextMenuItemTagLearnSpelling,
    kWKContextMenuItemTagOther,
    kWKContextMenuItemTagSearchInSpotlight,
    kWKContextMenuItemTagSearchWeb,
    kWKContextMenuItemTagLookUpInDictionary,
    kWKContextMenuItemTagOpenWithDefaultApplication,
    kWKContextMenuItemTagPDFActualSize,
    kWKContextMenuItemTagPDFZoomIn,
    kWKContextMenuItemTagPDFZoomOut,
    kWKContextMenuItemTagPDFAutoSize,
    kWKContextMenuItemTagPDFSinglePage,
    kWKContextMenuItemTagPDFFacingPages,
    kWKContextMenuItemTagPDFContinuous,
    kWKContextMenuItemTagPDFNextPage,
    kWKContextMenuItemTagPDFPreviousPage,
    kWKContextMenuItemTagOpenLink,
    kWKContextMenuItemTagIgnoreGrammar,
    kWKContextMenuItemTagSpellingMenu, 
    kWKContextMenuItemTagShowSpellingPanel,
    kWKContextMenuItemTagCheckSpelling,
    kWKContextMenuItemTagCheckSpellingWhileTyping,
    kWKContextMenuItemTagCheckGrammarWithSpelling,
    kWKContextMenuItemTagFontMenu, 
    kWKContextMenuItemTagShowFonts,
    kWKContextMenuItemTagBold,
    kWKContextMenuItemTagItalic,
    kWKContextMenuItemTagUnderline,
    kWKContextMenuItemTagOutline,
    kWKContextMenuItemTagStyles,
    kWKContextMenuItemTagShowColors,
    kWKContextMenuItemTagSpeechMenu, 
    kWKContextMenuItemTagStartSpeaking,
    kWKContextMenuItemTagStopSpeaking,
    kWKContextMenuItemTagWritingDirectionMenu, 
    kWKContextMenuItemTagDefaultDirection,
    kWKContextMenuItemTagLeftToRight,
    kWKContextMenuItemTagRightToLeft,
    kWKContextMenuItemTagPDFSinglePageScrolling,
    kWKContextMenuItemTagPDFFacingPagesScrolling,
    kWKContextMenuItemTagInspectElement,
    kWKContextMenuItemTagTextDirectionMenu,
    kWKContextMenuItemTagTextDirectionDefault,
    kWKContextMenuItemTagTextDirectionLeftToRight,
    kWKContextMenuItemTagTextDirectionRightToLeft,
    kWKContextMenuItemTagCorrectSpellingAutomatically,
    kWKContextMenuItemTagSubstitutionsMenu,
    kWKContextMenuItemTagShowSubstitutions,
    kWKContextMenuItemTagSmartCopyPaste,
    kWKContextMenuItemTagSmartQuotes,
    kWKContextMenuItemTagSmartDashes,
    kWKContextMenuItemTagSmartLinks,
    kWKContextMenuItemTagTextReplacement,
    kWKContextMenuItemTagTransformationsMenu,
    kWKContextMenuItemTagMakeUpperCase,
    kWKContextMenuItemTagMakeLowerCase,
    kWKContextMenuItemTagCapitalize,
    kWKContextMenuItemTagChangeBack,
    kWKContextMenuItemTagOpenMediaInNewWindow,
    kWKContextMenuItemTagCopyMediaLinkToClipboard,
    kWKContextMenuItemTagToggleMediaControls,
    kWKContextMenuItemTagToggleMediaLoop,
    kWKContextMenuItemTagEnterVideoFullscreen,
    kWKContextMenuItemTagMediaPlayPause,
    kWKContextMenuItemTagMediaMute,
    kWKContextMenuItemTagDictationAlternative,
/* #if ENABLE(TIZEN_CONTEXT_MENU_SELECT) */
    KWKContextMenuItemTagSelectAll,
    KWKContextMenuItemTagSelectWord,
/* #endif */
/* #if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_TEXT_SELECTION_MODE) */
    KWKContextMenuItemTagTextSelectionMode,
/* #endif */
/* #if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_CLIPBOARD) */
    KWKContextMenuItemTagClipboard,
/* #endif */
/* #if ENABLE(TIZEN_DRAG_SUPPORT) */
    kWKContextMenuItemTagDrag,
/* #endif */
/* #if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_TRANSLATE) */
    KWKContextMenuItemTagTranslate,
/* #endif */
/* #if ENABLE(TIZEN_WEBKIT2_COPY_MENU_FOR_SPECIFIC_SCHEME_LINK) */
    KWKContextMenuItemTagCopyLinkData,
/* #endif */
    kWKContextMenuItemNewTags = 9000,
/* #if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_QUICK_MEMO) */
    KWKContextMenuItemTagQuickMemo,
/* #endif */
/*#if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_OPEN_LINK_IN_BACKGROUND)*/
    kWKContextMenuItemTagOpenLinkInBackground,
/*#endif*/
    kWKContextMenuItemBaseApplicationTag = 10000
};
typedef uint32_t WKContextMenuItemTag;

enum {
    kWKContextMenuItemTypeAction,
    kWKContextMenuItemTypeCheckableAction,
    kWKContextMenuItemTypeSeparator,
    kWKContextMenuItemTypeSubmenu
};
typedef uint32_t WKContextMenuItemType;
    
#ifdef __cplusplus
}
#endif

#endif /* WKContextMenuItemTypes_h */
