#include "../common/basic/basic.cpp"
#include "../common/basic/random.cpp"
#include "../common/graphics/graphics.cpp"
#include "../common/ui/ui.cpp"

static bool isFullScreen;
static bool lineDrawing;
static bool rectDrawing;
static Point2i draw0;
static Font font;
static UIElement* e0;
static UIElement* scrollPane;
static UIElement* focused;
static UIElement* colorPicker1;
static UIElement* colorPicker2;

struct Edge {
	uint32 color;
	UIElement* p0;
	UIElement* p1;
	Edge* next;
	Edge* prev;
};

struct {
	Edge* first;
	Edge* last;
	FixedSize allocator;
} edges;

#define CONTEXT_LFT		1
#define CONTEXT_RGT		2
#define CONTEXT_UP 		3
#define CONTEXT_DN 		4
#define INTERSECTION 	5
#define CONTEXT_NODE 	6
#define CONTEXT_GROUP	7

Edge* GetLastConnectedEdge(UIElement* node) {
	for (Edge* edge = edges.last; edge != NULL; edge = edge->prev) {
		if (edge->p0 == node || edge->p1 == node) return edge;
	}
	return NULL;
}

void DeleteEdge(Edge* edge) {
	LINKEDLIST_REMOVE(&edges, edge);
	FixedSizeFree(&edges.allocator, edge);
}

void RemoveAllEdges(UIElement* p) {
	LINKEDLIST_FOREACH(&edges, Edge, edge) {
		if (edge->p0->parent == p || edge->p1->parent == p) DeleteEdge(edge);
	}
}

void AddEdge(UIElement* node0, UIElement* node1, uint32 color) {
	Edge* edge = (Edge*)FixedSizeAlloc(&edges.allocator);
	ASSERT(edge != NULL);
	edge->p0 = node0;
	edge->p1 = node1;
	edge->color = color;
	LINKEDLIST_ADD(&edges, edge);
}

void DestroyNode(UIElement* element) {
	ASSERT(element->context.i == CONTEXT_NODE);
	RemoveAllEdges(element);
	UIDestroyElement(element);
}

// UI Callbacks
//---------------

void ExitFullScreen(UIElement* e);

void EnterFullScreen(UIElement* e) {
	OSEnterFullScreen();
	isFullScreen = true;
	e->onClick = ExitFullScreen;
	e->image.crop = {0.25, 0.5, 0.5, 0.75};
	OSResetMouse();
}

void ExitFullScreen(UIElement* e) {
	OSExitFullScreen();
	isFullScreen = false;
	e->onClick = EnterFullScreen;
	e->image.crop = {0, 0.5, 0.25, 0.75};
	OSResetMouse();
}

void ClearAll(UIElement* e) {
	LINKEDLIST_FOREACH(scrollPane, UIElement, element) {
		if (element->context.i == CONTEXT_NODE)
			RemoveAllEdges(element);
		UIDestroyElement(element);
	}
}

void Visible(UIElement* e) {
	e->background = RGBA_BLUE;
	OSSetCursorIcon(CUR_HAND);
}

void DrawLine(UIElement* e) {
	if (lineDrawing) {
		if (e0 != e) 
			AddEdge(e0, e, colorPicker2->background);

		if (e->context.i == INTERSECTION && e0 != e) {
			e0 = e;
			Box2i box = UIGetAbsolutePosition(e);
			draw0 = {(box.x0+box.x1)/2, (box.y0+box.y1)/2};
		}
		else lineDrawing = false;
	}
	else {
		lineDrawing = true;
		e0 = e;
		Box2i box = UIGetAbsolutePosition(e);
		draw0 = {(box.x0+box.x1)/2, (box.y0+box.y1)/2};
	}
}

void SetSelected(UIElement* e) {
	UISelectTextElement(e->first);
	focused = e;
}

void SetFocused(UIElement* e) {
	focused = e;
}

void CreateSocket(UIElement* node, int32 x, int32 y, uint32 flag, int64 context) {
	UIElement* socket  = UICreateElement(node);
	socket->box = {x, y, 12, 12};
	socket->onHover = Visible;
	socket->onClick = DrawLine;
	socket->flags = UI_CLICKABLE | UI_ADDENDUM | flag;
	socket->context.i = context;
}

void CreateNode(int32 x, int32 y) {
	UIElement* node = UICreateElement(scrollPane);
	node->pos = {x, y};
	node->minDim = {120, 60};
	node->context.i = CONTEXT_NODE;
	node->flags = UI_RESIZABLE | UI_MOVABLE | UI_SHUFFLEABLE | UI_MIN_CONTENT;
	node->radius = 12;
	node->background = colorPicker1->background;
	node->borderColor = colorPicker2->background;
	node->borderWidth = 1;
	node->onMove = SetSelected;
	node->onResize = SetSelected;

	UIElement* textElement = UICreateElement(node);
	textElement->minDim = {36, 36};
	textElement->text.font = &font;
	textElement->text.color = colorPicker2->background;
	textElement->flags = UI_EDITABLE | UI_FIT_CONTENT | UI_CENTER | UI_MIDDLE;
	UISelectTextElement(textElement);

	CreateSocket(node, -6, 0, UI_MIDDLE, CONTEXT_LFT);
	CreateSocket(node, -6, 0, UI_MIDDLE | UI_RIGHT, CONTEXT_RGT);
	CreateSocket(node, 0, -6, UI_CENTER, CONTEXT_UP);
	CreateSocket(node, 0, -6, UI_CENTER | UI_BOTTOM, CONTEXT_DN);

	focused = node;
}

#if defined(__gnu_linux__)
extern byte* _binary_data_icons_bmp_start;
extern byte* _binary_data_nodes_png_start;
#endif

int main() {
	// Initialize stuff
	//---------------------
	Arena persist = CreateArena(1024*1024*128);
	Arena scratch = CreateArena(1024*1024*128);
	edges.allocator = CreateFixedSize(&persist, 1024, sizeof(Edge));
	lineDrawing = false;
	rectDrawing = false;
	e0 = NULL;
	focused = NULL;
	UIElement* copied = NULL;
	
	UIInit(&persist, &scratch);
	OSCreateWindow("a graph thingy", 1242, 768);
#if _WIN32
	Win32SetWindowIcon(2);
#elif defined(__gnu_linux__)
	LinuxSetWindowIcon(PNGLoadImage(&scratch, _binary_data_nodes_png_start));
#endif
	GfxInit(&scratch);
	UISetWindowElement(RGBA_DARKGREY);
	RandomInit(1234);
	font = LoadDefaultFont(&scratch, 24);
#if defined(_WIN32)
	TextureId iconAtlas = GfxLoadTexture(&scratch, LoadAsset(1).data, IMAGE_BITMAP, GFX_SMOOTH);
#elif defined(__gnu_linux__)
	TextureId iconAtlas = GfxLoadTexture(&scratch, _binary_data_icons_bmp_start, GFX_SMOOTH);
#endif

	scrollPane = UICreateElement(NULL);
	scrollPane->flags = UI_INFINITESCROLL;
	UIElement* fullButton;
	{
		int32 x = 12;
		UIElement* clearButton = UICreateElement(NULL);
		clearButton->pos = {x, 12};
		clearButton->dim = {24, 24};
		clearButton->flags = UI_CLICKABLE;
		clearButton->onClick = ClearAll;
		clearButton->name = STR("clear");
		clearButton->image.atlas = iconAtlas;
		clearButton->image.crop = {0.5, 0.75, 0.75, 1};

		x += 36;
		fullButton = UICreateElement(NULL);
		fullButton->pos = {x, 12};
		fullButton->dim = {24, 24};
		fullButton->flags = UI_CLICKABLE;
		fullButton->onClick = EnterFullScreen;
		fullButton->name = STR("enter/exit full screen");
		fullButton->image.atlas = iconAtlas;
		fullButton->image.crop = {0, 0.5, 0.25, 0.75};

		x += 36;
		colorPicker1 = UICreateColorDropdown(NULL, {24, 24}, {x, 12}, RGBA_DARKGREY, RGBA_WHITE);
		UIAddColorDropdownItem(colorPicker1, RGBA_BLACK);
		UIAddColorDropdownItem(colorPicker1, RGBA_LIGHTGREY);
		UIAddColorDropdownItem(colorPicker1, RGBA_GREY);
		UIAddColorDropdownItem(colorPicker1, RGBA_DARKGREY);
		UIElement* nocolor = UIAddColorDropdownItem(colorPicker1, 0);
		nocolor->symbol.type = UI_DIAGONAL;
		nocolor->symbol.color = RGBA_WHITE;
		nocolor->symbol.pos = nocolor->dim;

		x += 48;
		colorPicker2 = UICreateColorDropdown(NULL, {24, 24}, {x, 12}, RGBA_WHITE, RGBA_BLACK);
		UIAddColorDropdownItem(colorPicker2, RGBA_BLUE);
		UIAddColorDropdownItem(colorPicker2, RGBA_GREEN);
		UIAddColorDropdownItem(colorPicker2, RGBA_ORANGE);
		UIAddColorDropdownItem(colorPicker2, RGBA_RED);
		UIAddColorDropdownItem(colorPicker2, RGBA_LILAC);
		UIAddColorDropdownItem(colorPicker2, RGBA_WHITE);
	}

	// main loop
	//-----------------
	
	bool running = true;
	while (running) {
		ArenaFreeAll(&scratch);
		OSProcessWindowEvents();

		if (OSWindowDestroyed()) break;

		Point2i cursorPos = OSGetCursorPosition();
		int32 relx = cursorPos.x + scrollPane->scrollPos.x;
		int32 rely = cursorPos.y + scrollPane->scrollPos.y;
	
		scrollPane->dim = OSGetWindowDimensions();
		if (ui.selected) focused = ui.selected->parent;
		if (OSIsKeyPressed(KEY_ENTER) && !OSIsKeyDown(KEY_CTRL)) {
			if (ui.selected) {
				Box2i box = UIGetAbsolutePosition(ui.selected->parent);
				CreateNode(box.x0, box.y1 + 36);
			}
			else {
				int32 x = (int32)RandomUniform(0, 1000) + scrollPane->scrollPos.x;
				int32 y = (int32)RandomUniform(0, 400) + scrollPane->scrollPos.y;
				CreateNode(x, y);
			}
			OSResetTypedText();
		}
	
		if (OSIsKeyPressed(KEY_BACKSPACE)) {
			if (ui.selected && GetTextLength() == 0) {
				DestroyNode(ui.selected->parent);
				ui.selected = NULL;
			}
		}

		if (OSIsKeyPressed(KEY_ESC)) {
			if (ui.selected && GetTextLength() == 0) {
				DestroyNode(ui.selected->parent);
				ui.selected = NULL;
			}
			else running = false;
		}

		if (OSIsKeyPressed(KEY_DELETE) && focused) {
			if (focused->context.i == CONTEXT_GROUP) {
				LINKEDLIST_FOREACH(focused, UIElement, element) {
					Point2i absolute = GetAbsolutePosition(element).p0;
					element->pos = MOVE2(absolute, scrollPane->scrollPos);
					element->parent = scrollPane;
				}
				LINKEDLIST_CONCAT(scrollPane, focused);
				
				focused->first = NULL;
				UIDestroyElement(focused);
			}
			if (focused->context.i == CONTEXT_NODE) {
				DestroyNode(focused);
			}
		}
	
		String typed = OSGetTypedText();
		if (typed.length && ui.selected == NULL) {
			int32 x = (int32)RandomUniform(0, 1000) + scrollPane->scrollPos.x;
			int32 y = (int32)RandomUniform(0, 400) + scrollPane->scrollPos.y;
			CreateNode(x, y);
		}

		if (OSIsKeyPressed(KEY_C) && OSIsKeyDown(KEY_CTRL)) {
			if ((!ui.selected || ui.end == ui.start) && focused)
				copied = focused;
		}

		if (OSIsKeyPressed(KEY_V) && OSIsKeyDown(KEY_CTRL)) {
			if (copied) { // TODO: make sure there is no text to paste
				UIElement* pasted = UICloneElement(copied, scrollPane);
				pasted->x += 12;
				pasted->y += 12;
				copied = pasted;
				focused = pasted;
			}
		}
	
		UIElement* active = UIUpdateActiveElement();
		if (active == scrollPane) {
	
			if (OSIsMouseLeftButtonDown() && !rectDrawing && !lineDrawing) {
				rectDrawing = true;
				draw0 = cursorPos;
			}
	
			int32 absx = ABS(cursorPos.x - draw0.x);
			int32 absy = ABS(cursorPos.y - draw0.y);
			bool moved = absx >= 4 && absy >= 4 && rectDrawing;
			if (OSIsMouseLeftReleased()) {
				if (lineDrawing) {
					lineDrawing = false;
				}
				else if (moved) {
					rectDrawing = false;

					// Create grouping
					UIElement* group = UICreateElement(scrollPane);
					int32 x = draw0.x + scrollPane->scrollPos.x;
					int32 y = draw0.y + scrollPane->scrollPos.y;
					group->x = MIN(x, relx);
					group->y = MIN(y, rely);
					group->width = ABS(relx - x);
					group->height = ABS(rely - y);
					group->borderWidth = 1;
					group->borderColor = colorPicker2->background;
					group->background = colorPicker1->background;
					group->flags = UI_MOVABLE | UI_RESIZABLE;
					group->context.i = CONTEXT_GROUP;
					group->onMove = SetFocused;
					group->onResize = SetFocused;
					for (UIElement* element = scrollPane->first; element != NULL; ) {
						UIElement* next = element->next;
						if (element == group) {
							// do nothing
						}
						else if (group->x <= element->x && element->x+element->width <= group->x+group->width &&
							group->y <= element->y && element->y+element->height <= group->y+group->height) {
							Point2i pos = GetAbsolutePosition(element).p0;
							LINKEDLIST_REMOVE(scrollPane, element);
							element->parent = group;
							element->next = NULL;
							element->prev = NULL;
							LINKEDLIST_ADD(group, element);
							element->pos = GetRelativePosition(pos, group);
						}
						element = next;
					}
					focused = group;
				}
				else {
					CreateNode(relx - 60, rely - 30);
				}
			} 
	
			if (OSIsMouseRightClicked()) {
				lineDrawing = false;
				rectDrawing = false;
				focused = NULL;

				// Create Intersection
				UIElement* container = UICreateElement(scrollPane);
				container->pos = {relx - 8, rely - 8};
				container->dim = {16, 16};
				container->background = RGBA_WHITE;
				container->flags = UI_MOVABLE | UI_SHUFFLEABLE;
			
				UIElement* inter = UICreateElement(container);
				inter-> pos = {2, 2};
				inter->dim = {12, 12};
				inter->background = RGBA_BLUE;
				inter->onClick = DrawLine;
				inter->flags = UI_CLICKABLE | UI_MOVABLE;
				inter->context.i = INTERSECTION;
			}
		}
		else {
			if (OSIsMouseRightClicked()) {
				Edge* edge = GetLastConnectedEdge(active);
				if (edge) DeleteEdge(edge);
			}
			if (OSIsMouseLeftClicked()) {
				focused = active;
			}
			if (OSIsMouseLeftButtonUp() && rectDrawing) {
				rectDrawing = false;
			}
		}	
	
		// Rendering
		//-------------------

		GfxClearScreen();
		{
			{
				UIElement* temp = focused;
				if (temp) temp->borderWidth++;
				UIRenderElements();
				if (temp) temp->borderWidth--;
			}

			LINKEDLIST_FOREACH(&edges, Edge, edge) {
				Box2i box0 = UIGetAbsolutePosition(edge->p0);
				Box2i box1 = UIGetAbsolutePosition(edge->p1);
				Point2 p0 = {(float32)(box0.x0+box0.x1)/2.0f, UI_FLIPY((float32)(box0.y0+box0.y1)/2.0f)};
				Point2 p3 = {(float32)(box1.x0+box1.x1)/2.0f, UI_FLIPY((float32)(box1.y0+box1.y1)/2.0f)};
		
				Point2 p1;
				if (edge->p0->context.i == CONTEXT_LFT)
					p1 = {p0.x - 48.0f, p0.y};
				else if (edge->p0->context.i == CONTEXT_RGT)
					p1 = {p0.x + 48.0f, p0.y};
				else if (edge->p0->context.i == CONTEXT_DN)
					p1 = {p0.x, p0.y - 48.0f};
				else
					p1 = {p0.x, p0.y + 48.0f};
		
				Point2 p2;
				if (edge->p1->context.i == CONTEXT_LFT)
					p2 = {p3.x - 48.0f, p3.y};
				else if (edge->p1->context.i == CONTEXT_RGT)
					p2 = {p3.x + 48.0f, p3.y};
				else if (edge->p1->context.i == CONTEXT_DN)
					p2 = {p3.x, p3.y - 48.0f};
				else
					p2 = {p3.x, p3.y + 48.0f};
		
				if (edge->p0->context.i == INTERSECTION && edge->p1->context.i != INTERSECTION) {
					GfxDrawCurve3(p0, p2, p3, 3.0, edge->color);
				}
				else if (edge->p0->context.i != INTERSECTION && edge->p1->context.i == INTERSECTION) {
					GfxDrawCurve3(p0, p1, p3, 3.0, edge->color);
				}
				else if (edge->p0->context.i == INTERSECTION && edge->p1->context.i == INTERSECTION) {
					Point2 points[2] = {p0, p3};
					GfxDrawLine({points, 2}, 3.0, edge->color);
				}
				else
					GfxDrawCurve4(p0, p1, p2, p3, 3.0, edge->color);
			}

			if (!OSIsMouseLeftButtonDown() && lineDrawing) {
				UIDrawLine(draw0, cursorPos, 3, colorPicker2->background);
			}
		  	if (OSIsMouseLeftButtonDown() && rectDrawing) {
				UIDrawRect(draw0, cursorPos, 3, RGBA_WHITE);
			}
		}
		GfxSwapBuffers();
	}
	return 0;
}

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
extern "C" void WinMainCRTStartup() {
    ExitProcess(main());
}
#endif