/*  
 * BePDF: The PDF reader for Haiku.
 * 	 Copyright (C) 1997 Benoit Triquet.
 * 	 Copyright (C) 1998-2000 Hubert Figuiere.
 * 	 Copyright (C) 2000-2011 Michael Pfeiffer.
 * 	 Copyright (C) 2013 waddlesplash.
 * 	 Copyright (C) 2016 Adrián Arroyo Calle
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ctype.h>

#include <Box.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <Region.h>
#include <StringView.h>
#include <TabView.h>

#include "BePDF.h"
#include "FileInfoWindow.h"
#include "LayoutUtils.h"
#include "StringLocalization.h"
#include "TextConversion.h"


const int16 FileInfoWindow::noKeys = 9;

const char *FileInfoWindow::systemKeys[] = {
	"Author",
	"CreationDate",
	"ModDate",
	"Creator",
	"Producer",
	"Title",
	"Subject",
	"Keywords",
	"Trapped"
}; 

const char *FileInfoWindow::authorKey = systemKeys[0];
const char *FileInfoWindow::creationDateKey = systemKeys[1];
const char *FileInfoWindow::modDateKey = systemKeys[2];
const char *FileInfoWindow::creatorKey = systemKeys[3];
const char *FileInfoWindow::producerKey = systemKeys[4];
const char *FileInfoWindow::titleKey = systemKeys[5];
const char *FileInfoWindow::subjectKey = systemKeys[6];
const char *FileInfoWindow::keywordsKey = systemKeys[7];
const char *FileInfoWindow::trappedKey = systemKeys[8];

static const char *YesNo(bool yesNo) {
	return yesNo ? TRANSLATE("Yes") : TRANSLATE("No");
}

static const char *Allowed(bool allowed) {
	return allowed ? TRANSLATE("Allowed") : TRANSLATE("Denied");
}

#define COPYN(N) for (n = N; n && date[i]; n--) s[j++] = date[i++];

static int ToInt(const char *s, int i, int n) {
	int d = 0;
	for (; n && s[i] && isdigit(s[i]) ; n --, i ++) {
		d = 10 * d + (int)(s[i] - '0');
	}
	return d;
} 

// (D:YYYYMMDDHHmmSSOHH'mm')
static const char *ToDate(const char *date, time_t *time) {
static char s[80];
	struct tm d;
	memset(&d, sizeof(d), 0);

	if ((date[0] == 'D') && (date[1] == ':')) {
		int i = 2;
		// skip spaces
		while (date[i] == ' ') i++;
		int from = i;
		while (date[i] && isdigit(date[i])) i++;
		int to = i;
		int j = 0, n;
		i = from;
		d.tm_year = ToInt(date, i, 4) - 1900;
		if (to - from > 12) 
			COPYN(to - from - 10)
		else
			COPYN(to - from - 4);
		s[j++] = '/';
		d.tm_mon = ToInt(date, i, 2)-1;
		COPYN(2)
		s[j++] = '/';
		d.tm_mday = ToInt(date, i, 2);
		COPYN(2)
		s[j++] = ' ';
		if (date[i]) {
			d.tm_hour = ToInt(date, i, 2);
			COPYN(2);
			s[j++] = ':';
			d.tm_min = ToInt(date, i, 2);
			COPYN(2);
			s[j++] = ':';
			d.tm_sec = ToInt(date, i, 2);
			COPYN(2);
			if (date[i]) {
				int off = 0;
				int sign = 1;
				s[j++] = ' ';
				if (date[i] == '-') sign = -1;
				s[j++] = date[i++];
				off = ToInt(date, i, 2) * 3600;
				COPYN(2); i++; // skip '
				s[j++] = ':';
				off += ToInt(date, i, 2) * 60;
				COPYN(2);
				d.tm_gmtoff = off * sign;
			}
		}
		s[j] = 0;
		if (time) *time = mktime(&d);
		return s;
	}
	else
		return date;
}

bool FileInfoWindow::IsSystemKey(const char *key) {
	for (int i = 0; i < noKeys; i++) {
		if (strcmp(key, systemKeys[i]) == 0) return true;
	}
	return false;
}

BString *FileInfoWindow::GetProperty(Dict *dict, const char *key, time_t *time) {
	Object obj, *item;
	BString *result = NULL;
	if (time) *time = 0;
	
	if ((item = dict->lookup((char*)key, &obj)) != NULL) {
		ObjType type = item->getType();
		if (type == objString) {
			GString *string = item->getString();
			const char *s = string->getCString();
			const char *date = ToDate(s, time);
			if (date != s) {
				result = TextToUtf8(date, strlen(date));
			} else {
				result = TextToUtf8(s, string->getLength());
			}
		}
	}
	obj.free();
	return result;
}

void FileInfoWindow::AddPair(BGridView *dest, BView *lv, BView *rv) {
	BGridLayout *layout = dest->GridLayout();
	int32 nextRow = layout->CountRows() + 1;
	layout->AddView(lv, 1, nextRow);
	layout->AddView(rv, 2, nextRow);
}

void FileInfoWindow::CreateProperty(BGridView *view, Dict *dict, const char *key, const char *title) {
	time_t time;
	BString *string = GetProperty(dict, key, &time);
	AddPair(view, new BStringView("", title),
		new BStringView("", (string) ? string->String() : "-"));
	delete string;
}

bool FileInfoWindow::AddFont(BList *list, GfxFont *font) {
	// Font already in list?
	Font **ids = (Font**)list->Items();
	for (int i = 0; i < list->CountItems(); i++) {
		if (((ids[i]->ref.num == font->getID()->num) &&
			(ids[i]->ref.gen == font->getID()->gen))) return false;
	}
	// Add font to list
	list->AddItem(new Font(*font->getID(), font->getName()));
	return true;
}

static void GetGString(BString &s, GString *g) {
	if (g) {
		BString *utf8 = TextToUtf8(g->getCString(), g->getLength());
		s = *utf8;
		delete utf8;
	}
}


// FontItem 
class FontItem : public BRow
{
public:
	FontItem(uint32 level, bool superitem, bool expanded, const char *name, const char *embName, const char *type);
	~FontItem() {};
	void DrawItemColumn(BView* owner, BRect item_column_rect, int32 column_index, bool complete);
	void Update(BView *owner, const BFont *font);

private:
	static rgb_color kHighlight, kRedColor, kDimRedColor, kBlackColor, kMedGray;
	BString fText[3];
	float fTextOffset;
};

rgb_color FontItem::kHighlight = {128, 128, 128, 0},
	FontItem::kRedColor = {255, 0, 0, 0},
	FontItem::kDimRedColor = {128, 0, 0, 0},
	FontItem::kBlackColor = {0, 0, 0, 0},
	FontItem::kMedGray = {192, 192, 192, 0};
	
FontItem::FontItem(uint32 level, bool superitem, bool expanded, const char *name, const char *embName, const char *type) 
: BRow() {
	fText[0] = name; 
	fText[1] = embName;
	fText[2] = type;
}

void FontItem::DrawItemColumn(BView* owner, BRect item_column_rect, int32 column_index, bool complete)
{
	rgb_color color;
	rgb_color White = {255, 255, 255};
	rgb_color BeListSelectGrey = {128, 128, 128};
	rgb_color Black = {0, 0, 0};
	bool selected = IsSelected();
	if(selected)
		color = BeListSelectGrey;
	else
		color = White;
	owner->SetLowColor(color);
	owner->SetDrawingMode(B_OP_COPY);
	if(selected || complete)
	{
		owner->SetHighColor(color);
		owner->FillRect(item_column_rect);
	}
	BRegion Region;
	Region.Include(item_column_rect);
	owner->ConstrainClippingRegion(&Region);
	if (column_index == 2) {
		owner->SetHighColor(selected ? kDimRedColor : kRedColor);
	} else {
		owner->SetHighColor(Black);
	}
	if(column_index >= 0)
		owner->DrawString(fText[column_index].String(),
			BPoint(item_column_rect.left+2.0,item_column_rect.top+fTextOffset));
	owner->ConstrainClippingRegion(NULL);
}


void FontItem::Update(BView *owner, const BFont *font)
{
	font_height FontAttributes;
	be_plain_font->GetHeight(&FontAttributes);
	float FontHeight = ceil(FontAttributes.ascent) + ceil(FontAttributes.descent);
	fTextOffset = ceil(FontAttributes.ascent) + (Height()-FontHeight)/2.0;
}


BRow *FileInfoWindow::FontItem(GfxFont *font) {
	BString name;
	GetGString(name, font->getName());

	BString embName; 
	if (font->getEmbeddedFontName()) {
		const char* name = font->getEmbeddedFontName()->getCString();
		BString *utf8 = TextToUtf8(name, font->getEmbeddedFontName()->getLength());
		embName = *utf8;
		delete utf8;
	}

	BString type;
	switch (font->getType()) {
	case fontUnknownType: type = "Unknown Type";
		break;
	case fontType1: type = "Type 1";
		break;
	case fontType1C: type = "Type 1C";
		break;
	case fontType3: type = "Type 3";
		break;
	case fontTrueType: type = "TrueType";
		break;
	case fontCIDType0: type = "CID Type 0";
		break;
	case fontCIDType0C: type = "CID Type 0C";
		break;
	case fontCIDType2: type = "CID Type 2";
		break;
	case fontType1COT: type = "Type 1C OpenType";
		break;
	case fontTrueTypeOT: type = "TrueType 0 OpenType";
		break;
	case fontCIDType0COT: type = "CID Type 0C OpenType";
		break;
	case fontCIDType2OT: type = "CID Type2 OpenType";
		break;
	}
	return new ::FontItem(0, false, false, name.String(), embName.String(), type.String());	
}

void FileInfoWindow::QueryFonts(PDFDoc *doc, int page) {

	Catalog *catalog = doc->getCatalog();
	GfxFontDict *gfxFontDict;
	GfxFont *font;

	// remove items from font list
	Lock();
	mFontList->Clear();
	Unlock();
		
	BList fontList;
	int first, last;
	if (page == 0) {
		first = 1; last = doc->getNumPages();
	} else {
		first = last = page;
	}

	for (int pg = first; pg <= last; pg++) {
		if ((mState == STOP) || (mState == QUIT)) break;
		
		Page *page = catalog->getPage(pg);
		Dict *resDict;
		if ((resDict = page->getResourceDict()) != NULL) {
			Object fontDict;
			resDict->lookup("Font", &fontDict);
			if (fontDict.isDict()) {
				// TODO check if ref to Font dict has to be passed in!
				gfxFontDict = new GfxFontDict(doc->getXRef(), NULL, fontDict.getDict());
				for (int i = 0; i < gfxFontDict->getNumFonts(); i++) {
					font = gfxFontDict->getFont(i);
					if (font != NULL && AddFont(&fontList, font)) {
						Lock();
						mFontList->AddRow(FontItem(font));
						Unlock();
					}
				}
				delete gfxFontDict;
			}
			fontDict.free();
		}	
	}
	
	Font **ids = (Font**)fontList.Items();
	for (int i = 0; i < fontList.CountItems(); i++) {
		delete ids[i];
	}
	return ;
}

void FileInfoWindow::Refresh(BEntry *file, PDFDoc *doc, int page) {
	PDFLock lock;

	mState = NORMAL;

	BTabView *tabs = new BTabView("tabs");

	BGridView *document = new BGridView();

	BPath path;
	if (file->GetPath(&path) == B_OK) {
		AddPair(document, new BStringView("", TRANSLATE("Filename:")),
			new BStringView("", path.Leaf()));
		AddPair(document, new BStringView("", TRANSLATE("Path:")),
			new BStringView("", path.Path()));
	}

	Object obj;
	if (doc->getDocInfo(&obj) && obj.isDict()) {
		Dict *dict = obj.getDict();
		
		CreateProperty(document, dict, titleKey, TRANSLATE("Title:"));
		CreateProperty(document, dict, subjectKey, TRANSLATE("Subject:"));
		CreateProperty(document, dict, authorKey, TRANSLATE("Author:"));
		CreateProperty(document, dict, keywordsKey, TRANSLATE("Keywords:"));
		CreateProperty(document, dict, creatorKey, TRANSLATE("Creator:"));
		CreateProperty(document, dict, producerKey, TRANSLATE("Producer:"));
		CreateProperty(document, dict, creationDateKey, TRANSLATE("Created:"));
		CreateProperty(document, dict, modDateKey, TRANSLATE("Modified:"));

		for (int i = 0; i < dict->getLength(); i++) {
			if (!IsSystemKey(dict->getKey(i))) {
				BString title(dict->getKey(i));
				title << ":";
				CreateProperty(document, dict, dict->getKey(i), title.String());
			}
		}
	}
    obj.free();
    
	char ver[80];
	sprintf(ver, "%.1f", doc->getPDFVersion());
	AddPair(document, new BStringView("", TRANSLATE("Version:")), new BStringView("", ver));

	AddPair(document, new BStringView("", TRANSLATE("Linearized:")), 
		new BStringView("", YesNo(doc->isLinearized())));

	BView *docView = new BView("Document", 0);
	BLayoutBuilder::Group<>(docView, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL)
			.Add(document)
			.AddGlue()
		.End()
		.AddGlue();

	tabs->AddTab(docView);

	// Security
	BGridView *security = new BGridView();

	AddPair(security, new BStringView("", TRANSLATE("Encrypted:")), new BStringView("", YesNo(doc->isEncrypted())));
	AddPair(security, new BStringView("", TRANSLATE("Printing:")), new BStringView("", Allowed(doc->okToPrint())));
	AddPair(security, new BStringView("", TRANSLATE("Editing:")), new BStringView("", Allowed(doc->okToChange())));
	AddPair(security, new BStringView("", TRANSLATE("Copy & paste:")), new BStringView("", Allowed(doc->okToCopy())));
	AddPair(security, new BStringView("", TRANSLATE("Annotations:")), new BStringView("", Allowed(doc->okToAddNotes())));

	BView *secView = new BView("Security", 0);
	BLayoutBuilder::Group<>(secView, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL)
			.Add(security)
			.AddGlue()
		.End()
		.AddGlue();

	tabs->AddTab(secView);

	// Fonts
	mFontList = new BColumnListView(BRect(0, 0, 100, 100), NULL, B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE, B_NO_BORDER,true);
	mFontList->AddColumn(new BStringColumn(TRANSLATE("Name"), 150.0, 150.0,150.0,true),0);
	mFontList->AddColumn(new BStringColumn(TRANSLATE("Embedded Name"), 150.0,150.0,150.0,true),1);
	mFontList->AddColumn(new BStringColumn(TRANSLATE("Type"), 80.0,80.0,80.0,true),2);

	mFontsBorder = new BBox("border");
	mFontsBorder->SetLabel(TRANSLATE("Fonts of this page"));
	mFontsBorder->AddChild(mFontList);

	mShowAllFonts = new BButton("showAllFonts", TRANSLATE("Show all fonts"), new BMessage(SHOW_ALL_FONTS_MSG));
	mStop = new BButton("stop", TRANSLATE("Abort"), new BMessage(STOP_MSG));

	BView *fonts = new BView("Fonts", 0);

	BLayoutBuilder::Group<>(fonts, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(mFontsBorder)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(mShowAllFonts)
			.Add(mStop)
		.End();

	tabs->AddTab(fonts);

	mStop->SetEnabled(false);
	QueryFonts(doc, page);

	BLayoutBuilder::Group<>(this)
		.Add(tabs);

	Show();
}

void FileInfoWindow::RefreshFontList(BEntry *file, PDFDoc *doc, int page) {
	if (mState == NORMAL) {
		QueryFonts(doc, page);
	}
}

void FileInfoWindow::QueryAllFonts(PDFDoc *doc) {
	QueryFonts(doc, 0);
	PostMessage(FONT_QUERY_STOPPED_NOTIFY);
}

FileInfoWindow::FileInfoWindow(GlobalSettings *settings, BEntry *file, PDFDoc *doc,
	BLooper *looper, int page)
	: BWindow(BRect(0, 0, 100, 100), TRANSLATE("File Info"),
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_AUTO_UPDATE_SIZE_LIMITS),
		mLooper(looper), mSettings(settings), mState(NORMAL) {

	AddCommonFilter(new EscapeMessageFilter(this, B_QUIT_REQUESTED));

	MoveTo(settings->GetFileInfoWindowPosition());
	float w, h;
	settings->GetFileInfoWindowSize(w, h);
	ResizeTo(w, h);
	mView = NULL;
	
	Refresh(file, doc, page);
}

bool FileInfoWindow::QuitRequested() {
	if ((mState == NORMAL) || (mState == ALL_FONTS)) {
		if (mLooper) {
			BMessage msg(QUIT_NOTIFY);
			mLooper->PostMessage(&msg);
		}
		return true;
	} else {
		if (mState == QUERY_ALL_FONTS) {
			mStop->SetEnabled(false); mState = QUIT;
		}
		return false;
	}
}

void FileInfoWindow::FrameMoved(BPoint p) {
	mSettings->SetFileInfoWindowPosition(p);
	BWindow::FrameMoved(p);
}

void FileInfoWindow::FrameResized(float w, float h) {
	mSettings->SetFileInfoWindowSize(w, h);
	BWindow::FrameResized(w, h);
}

void FileInfoWindow::MessageReceived(BMessage *msg) {
	switch (msg->what) {
	case SHOW_ALL_FONTS_MSG: 
		mState = QUERY_ALL_FONTS;
		mShowAllFonts->SetEnabled(false);
		mStop->SetEnabled(true);
		mFontsBorder->SetLabel(TRANSLATE("Searching all fonts…"));
		if (mLooper) {
			// do searching in another thread (= thread of window)
			mLooper->PostMessage(START_QUERY_ALL_FONTS_MSG);
		}
		break;
	case STOP_MSG:
		if (mState == QUERY_ALL_FONTS) {
			mStop->SetEnabled(false); mState = STOP;
		}
		break;
	case FONT_QUERY_STOPPED_NOTIFY:
		switch (mState) {
		case STOP: 
			mState = NORMAL;
			mShowAllFonts->SetEnabled(true);			
			mFontsBorder->SetLabel(TRANSLATE("All fonts of this document (aborted)"));
			break;
		case QUERY_ALL_FONTS:
			mState = ALL_FONTS;	mStop->SetEnabled(false);
			mFontsBorder->SetLabel(TRANSLATE("All fonts of this document"));		
			break;
		case QUIT:
			mState = NORMAL; PostMessage(B_QUIT_REQUESTED);
			break;
		 default:
		 	break; // empty
		}
		break;
	default:
		BWindow::MessageReceived(msg);
	}
}

