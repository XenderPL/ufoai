/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

//
// Surface Dialog
//
// Leonardo Zide (leo@lokigames.com)
//

#include "surfacedialog.h"

#include "debugging/debugging.h"

#include "iscenegraph.h"
#include "itexdef.h"
#include "iundo.h"
#include "iselection.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkspinbutton.h>
#include <gdk/gdkkeysyms.h>

#include "signal/isignal.h"
#include "generic/object.h"
#include "math/vector.h"
#include "texturelib.h"
#include "shaderlib.h"
#include "stringio.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/entry.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/pointer.h"
#include "gtkutil/button.h"
#include "map.h"
#include "select.h"
#include "brushmanip.h"
#include "preferences.h"
#include "brush_primit.h"
#include "xywindow.h"
#include "mainframe.h"
#include "gtkdlgs.h"
#include "dialog.h"
#include "brush.h"
#include "commands.h"
#include "stream/stringstream.h"
#include "grid.h"
#include "textureentry.h"

inline void spin_button_set_step(GtkSpinButton* spin, gfloat step) {
	gtk_spin_button_get_adjustment(spin)->step_increment = step;
}

class Increment {
	float& m_f;
public:
	GtkSpinButton* m_spin;
	GtkEntry* m_entry;
	Increment(float& f) : m_f(f), m_spin(0), m_entry(0) {
	}
	void cancel() {
		entry_set_float(m_entry, m_f);
	}
	typedef MemberCaller<Increment, &Increment::cancel> CancelCaller;
	void apply() {
		m_f = static_cast<float>(entry_get_float(m_entry));
		spin_button_set_step(m_spin, m_f);
	}
	typedef MemberCaller<Increment, &Increment::apply> ApplyCaller;
};

void SurfaceInspector_GridChange();

class SurfaceInspector : public Dialog {
	GtkWindow* BuildDialog();

	NonModalEntry m_textureEntry;
	NonModalSpinner m_hshiftSpinner;
	NonModalEntry m_hshiftEntry;
	NonModalSpinner m_vshiftSpinner;
	NonModalEntry m_vshiftEntry;
	NonModalSpinner m_hscaleSpinner;
	NonModalEntry m_hscaleEntry;
	NonModalSpinner m_vscaleSpinner;
	NonModalEntry m_vscaleEntry;
	NonModalSpinner m_rotateSpinner;
	NonModalEntry m_rotateEntry;

	IdleDraw m_idleDraw;

	GtkCheckButton* m_surfaceFlags[32];
	GtkCheckButton* m_contentFlags[32];

	NonModalEntry m_valueEntry;
	GtkEntry* m_valueEntryWidget;
public:
	WindowPositionTracker m_positionTracker;
	WindowPositionTrackerImportStringCaller m_importPosition;
	WindowPositionTrackerExportStringCaller m_exportPosition;

	// Dialog Data
	float m_fitHorizontal;
	float m_fitVertical;

	Increment m_hshiftIncrement;
	Increment m_vshiftIncrement;
	Increment m_hscaleIncrement;
	Increment m_vscaleIncrement;
	Increment m_rotateIncrement;
	GtkEntry* m_texture;

	SurfaceInspector() :
			m_textureEntry(ApplyShaderCaller(*this), UpdateCaller(*this)),
			m_hshiftSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)),
			m_hshiftEntry(Increment::ApplyCaller(m_hshiftIncrement), Increment::CancelCaller(m_hshiftIncrement)),
			m_vshiftSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)),
			m_vshiftEntry(Increment::ApplyCaller(m_vshiftIncrement), Increment::CancelCaller(m_vshiftIncrement)),
			m_hscaleSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)),
			m_hscaleEntry(Increment::ApplyCaller(m_hscaleIncrement), Increment::CancelCaller(m_hscaleIncrement)),
			m_vscaleSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)),
			m_vscaleEntry(Increment::ApplyCaller(m_vscaleIncrement), Increment::CancelCaller(m_vscaleIncrement)),
			m_rotateSpinner(ApplyTexdefCaller(*this), UpdateCaller(*this)),
			m_rotateEntry(Increment::ApplyCaller(m_rotateIncrement), Increment::CancelCaller(m_rotateIncrement)),
			m_idleDraw(UpdateCaller(*this)),
			m_valueEntry(ApplyFlagsCaller(*this), UpdateCaller(*this)),
			m_importPosition(m_positionTracker),
			m_exportPosition(m_positionTracker),
			m_hshiftIncrement(g_si_globals.shift[0]),
			m_vshiftIncrement(g_si_globals.shift[1]),
			m_hscaleIncrement(g_si_globals.scale[0]),
			m_vscaleIncrement(g_si_globals.scale[1]),
			m_rotateIncrement(g_si_globals.rotate) {
		m_fitVertical = 1;
		m_fitHorizontal = 1;
		m_positionTracker.setPosition(c_default_window_pos);
	}

	void constructWindow(GtkWindow* main_window) {
		m_parent = main_window;
		Create();
		AddGridChangeCallback(FreeCaller<SurfaceInspector_GridChange>());
	}
	void destroyWindow() {
		Destroy();
	}
	bool visible() const {
		return GTK_WIDGET_VISIBLE(const_cast<GtkWindow*>(GetWidget()));
	}
	void queueDraw() {
		if (visible()) {
			m_idleDraw.queueDraw();
		}
	}

	void Update();
	typedef MemberCaller<SurfaceInspector, &SurfaceInspector::Update> UpdateCaller;
	void ApplyShader();
	typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyShader> ApplyShaderCaller;
	void ApplyTexdef();
	typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyTexdef> ApplyTexdefCaller;
	void ApplyFlags();
	typedef MemberCaller<SurfaceInspector, &SurfaceInspector::ApplyFlags> ApplyFlagsCaller;
};

namespace {
SurfaceInspector* g_SurfaceInspector;

inline SurfaceInspector& getSurfaceInspector() {
	ASSERT_NOTNULL(g_SurfaceInspector);
	return *g_SurfaceInspector;
}
}

void SurfaceInspector_constructWindow(GtkWindow* main_window) {
	getSurfaceInspector().constructWindow(main_window);
}
void SurfaceInspector_destroyWindow() {
	getSurfaceInspector().destroyWindow();
}

void SurfaceInspector_queueDraw() {
	getSurfaceInspector().queueDraw();
}

namespace {
CopiedString g_selectedShader;
TextureProjection g_selectedTexdef;
ContentsFlagsValue g_selectedFlags;
size_t g_selectedShaderSize[2];
}

void SurfaceInspector_SetSelectedShader(const char* shader) {
	g_selectedShader = shader;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SetSelectedTexdef(const TextureProjection& projection) {
	g_selectedTexdef = projection;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SetSelectedFlags(const ContentsFlagsValue& flags) {
	g_selectedFlags = flags;
	SurfaceInspector_queueDraw();
}

static bool s_texture_selection_dirty = false;

void SurfaceInspector_updateSelection() {
	s_texture_selection_dirty = true;
	SurfaceInspector_queueDraw();
}

void SurfaceInspector_SelectionChanged(const Selectable& selectable) {
	SurfaceInspector_updateSelection();
}

void SurfaceInspector_SetCurrent_FromSelected() {
	if (s_texture_selection_dirty == true) {
		s_texture_selection_dirty = false;
		if (!g_SelectedFaceInstances.empty()) {
			TextureProjection projection;
			Scene_BrushGetTexdef_Component_Selected(GlobalSceneGraph(), projection);

			SurfaceInspector_SetSelectedTexdef(projection);

			Scene_BrushGetShaderSize_Component_Selected(GlobalSceneGraph(), g_selectedShaderSize[0], g_selectedShaderSize[1]);
			g_selectedTexdef.m_brushprimit_texdef.coords[0][2] = float_mod(g_selectedTexdef.m_brushprimit_texdef.coords[0][2], (float)g_selectedShaderSize[0]);
			g_selectedTexdef.m_brushprimit_texdef.coords[1][2] = float_mod(g_selectedTexdef.m_brushprimit_texdef.coords[1][2], (float)g_selectedShaderSize[1]);

			CopiedString name;
			Scene_BrushGetShader_Component_Selected(GlobalSceneGraph(), name);
			if (string_not_empty(name.c_str())) {
				SurfaceInspector_SetSelectedShader(name.c_str());
			}

			ContentsFlagsValue flags;
			Scene_BrushGetFlags_Component_Selected(GlobalSceneGraph(), flags);
			SurfaceInspector_SetSelectedFlags(flags);
		} else {
			TextureProjection projection;
			Scene_BrushGetTexdef_Selected(GlobalSceneGraph(), projection);
			SurfaceInspector_SetSelectedTexdef(projection);

			CopiedString name;
			Scene_BrushGetShader_Selected(GlobalSceneGraph(), name);
			if (string_not_empty(name.c_str())) {
				SurfaceInspector_SetSelectedShader(name.c_str());
			}

			ContentsFlagsValue flags(0, 0, 0, false);
			Scene_BrushGetFlags_Selected(GlobalSceneGraph(), flags);
			SurfaceInspector_SetSelectedFlags(flags);
		}
	}
}

const char* SurfaceInspector_GetSelectedShader() {
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedShader.c_str();
}

const TextureProjection& SurfaceInspector_GetSelectedTexdef() {
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedTexdef;
}

const ContentsFlagsValue& SurfaceInspector_GetSelectedFlags() {
	SurfaceInspector_SetCurrent_FromSelected();
	return g_selectedFlags;
}

/*
===================================================
SURFACE INSPECTOR
===================================================
*/

si_globals_t g_si_globals;


/**
 * @brief make the shift increments match the grid settings
 * the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
 * this depends on a scale value if you have selected a particular texture on which you want it to work:
 * we move the textures in pixels, not world units. (i.e. increment values are in pixel)
 * depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
 * @code increment * scale = gridsize @endcode
 * hscale and vscale are optional parameters, if they are zero they will be set to the default scale
 * @note the default scale is 0.5f (128 pixels cover 64 world units)
 */
static void DoSnapTToGrid(float hscale, float vscale) {
	g_si_globals.shift[0] = static_cast<float>(float_to_integer(static_cast<float>(GetGridSize()) / hscale));
	g_si_globals.shift[1] = static_cast<float>(float_to_integer(static_cast<float>(GetGridSize()) / vscale));
	getSurfaceInspector().queueDraw();
}

void SurfaceInspector_GridChange() {
	if (g_si_globals.m_bSnapTToGrid)
		DoSnapTToGrid(Texdef_getDefaultTextureScale(), Texdef_getDefaultTextureScale());
}

/**
 * @brief make the shift increments match the grid settings
 * the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
 * this depends on the current texture scale used?
 * we move the textures in pixels, not world units. (i.e. increment values are in pixel)
 * depending on the texture scale it doesn't take the same amount of pixels to move of GetGridSize()
 * @code increment * scale = gridsize @endcode
 */
static void OnBtnMatchGrid(GtkWidget *widget, gpointer data) {
	float hscale, vscale;
	hscale = static_cast<float>(gtk_spin_button_get_value_as_float(getSurfaceInspector().m_hscaleIncrement.m_spin));
	vscale = static_cast<float>(gtk_spin_button_get_value_as_float(getSurfaceInspector().m_vscaleIncrement.m_spin));

	if (hscale == 0.0f || vscale == 0.0f) {
		globalOutputStream() << "ERROR: unexpected scale == 0.0f\n";
		return;
	}

	DoSnapTToGrid (hscale, vscale);
}

/**
 * @brief DoSurface will always try to show the surface inspector
 * or update it because something new has been selected
 * @note Shamus: It does get called when the SI is hidden, but not when you select something new. ;-)
 */
void DoSurface (void) {
	if (getSurfaceInspector().GetWidget() == 0) {
		getSurfaceInspector().Create();
	}
	getSurfaceInspector().Update();
	getSurfaceInspector().importData();
	getSurfaceInspector().ShowDlg();
}

void SurfaceInspector_toggleShown() {
	if (getSurfaceInspector().visible()) {
		getSurfaceInspector().HideDlg();
	} else {
		DoSurface();
	}
}

void SurfaceInspector_FitTexture() {
	UndoableCommand undo("textureAutoFit");
	Select_FitTexture(getSurfaceInspector().m_fitHorizontal, getSurfaceInspector().m_fitVertical);
}

static void OnBtnAxial(GtkWidget *widget, gpointer data) {
	UndoableCommand undo("textureDefault");
	TextureProjection projection;
	TexDef_Construct_Default(projection);

	Select_SetTexdef(projection);
}

static void OnBtnFaceFit(GtkWidget *widget, gpointer data) {
	getSurfaceInspector().exportData();
	SurfaceInspector_FitTexture();
}

static const char* surfaceflagNamesDefault[32] = {
	"surf1",
	"surf2",
	"surf3",
	"surf4",
	"surf5",
	"surf6",
	"surf7",
	"surf8",
	"surf9",
	"surf10",
	"surf11",
	"surf12",
	"surf13",
	"surf14",
	"surf15",
	"surf16",
	"surf17",
	"surf18",
	"surf19",
	"surf20",
	"surf21",
	"surf22",
	"surf23",
	"surf24",
	"surf25",
	"surf26",
	"surf27",
	"surf28",
	"surf29",
	"surf30",
	"surf31",
	"surf32"
};

static const char* contentflagNamesDefault[32] = {
	"cont1",
	"cont2",
	"cont3",
	"cont4",
	"cont5",
	"cont6",
	"cont7",
	"cont8",
	"cont9",
	"cont10",
	"cont11",
	"cont12",
	"cont13",
	"cont14",
	"cont15",
	"cont16",
	"cont17",
	"cont18",
	"cont19",
	"cont20",
	"cont21",
	"cont22",
	"cont23",
	"cont24",
	"cont25",
	"cont26",
	"cont27",
	"cont28",
	"cont29",
	"cont30",
	"cont31",
	"cont32"
};

const char* getSurfaceFlagName(std::size_t bit) {
	const char* value = g_pGameDescription->getKeyValue(surfaceflagNamesDefault[bit]);
	if (string_empty(value)) {
		return surfaceflagNamesDefault[bit];
	}
	return value;
}

const char* getContentFlagName(std::size_t bit) {
	const char* value = g_pGameDescription->getKeyValue(contentflagNamesDefault[bit]);
	if (string_empty(value)) {
		return contentflagNamesDefault[bit];
	}
	return value;
}


// =============================================================================
// SurfaceInspector class

guint togglebutton_connect_toggled(GtkToggleButton* button, const Callback& callback) {
	return g_signal_connect_swapped(G_OBJECT(button), "toggled", G_CALLBACK(callback.getThunk()), callback.getEnvironment());
}

GtkWindow* SurfaceInspector::BuildDialog() {
	GtkWindow* window = create_floating_window("Surface Inspector", m_parent);

	m_positionTracker.connect(window);

	global_accel_connect_window(window);

	window_connect_focus_in_clear_focus_widget(window);

	{
		// replaced by only the vbox:
		GtkWidget* vbox = gtk_vbox_new (FALSE, 5);
		gtk_widget_show (vbox);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

		{
			GtkWidget* hbox2 = gtk_hbox_new (FALSE, 5);
			gtk_widget_show (hbox2);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox2), FALSE, FALSE, 0);

			{
				GtkWidget* label = gtk_label_new ("Texture");
				gtk_widget_show (label);
				gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_box_pack_start(GTK_BOX(hbox2), GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_texture = entry;
				m_textureEntry.connect(entry);
				GlobalTextureEntryCompletion::instance().connect(entry);
			}
		}


		{
			GtkWidget* table = gtk_table_new (6, 4, FALSE);
			gtk_widget_show (table);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
			gtk_table_set_row_spacings(GTK_TABLE(table), 5);
			gtk_table_set_col_spacings(GTK_TABLE(table), 5);
			{
				GtkWidget* label = gtk_label_new ("Horizontal shift");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 8)), 0, 2));
				m_hshiftIncrement.m_spin = spin;
				m_hshiftSpinner.connect(spin);
				gtk_widget_show(GTK_WIDGET(spin));
				gtk_table_attach (GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 0, 1,
				                  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				                  (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(spin), 60, -2);
			}
			{
				GtkWidget* label = gtk_label_new ("Step");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 0, 1,
				                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(entry), 50, -2);
				m_hshiftIncrement.m_entry = entry;
				m_hshiftEntry.connect(entry);
			}
			{
				GtkWidget* label = gtk_label_new ("Vertical shift");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 8)), 0, 2));
				m_vshiftIncrement.m_spin = spin;
				m_vshiftSpinner.connect(spin);
				gtk_widget_show(GTK_WIDGET(spin));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 1, 2,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(spin), 60, -2);
			}
			{
				GtkWidget* label = gtk_label_new ("Step");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 1, 2,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(entry), 50, -2);
				m_vshiftIncrement.m_entry = entry;
				m_vshiftEntry.connect(entry);
			}
			{
				GtkWidget* label = gtk_label_new ("Horizontal stretch");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 8)), 0, 5));
				m_hscaleIncrement.m_spin = spin;
				m_hscaleSpinner.connect(spin);
				gtk_widget_show(GTK_WIDGET(spin));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 2, 3,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(spin), 60, -2);
			}
			{
				GtkWidget* label = gtk_label_new ("Step");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 2, 3, 2, 3,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 2, 3);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 2, 3,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 2, 3);
				gtk_widget_set_usize(GTK_WIDGET(entry), 50, -2);
				m_hscaleIncrement.m_entry = entry;
				m_hscaleEntry.connect(entry);
			}
			{
				GtkWidget* label = gtk_label_new ("Vertical stretch");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 8)), 0, 5));
				m_vscaleIncrement.m_spin = spin;
				m_vscaleSpinner.connect(spin);
				gtk_widget_show(GTK_WIDGET(spin));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 3, 4,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(spin), 60, -2);
			}
			{
				GtkWidget* label = gtk_label_new ("Step");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 2, 3, 3, 4,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 3, 4,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(entry), 50, -2);
				m_vscaleIncrement.m_entry = entry;
				m_vscaleEntry.connect(entry);
			}
			{
				GtkWidget* label = gtk_label_new ("Rotate");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkSpinButton* spin = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -8192, 8192, 2, 8, 8)), 0, 2));
				m_rotateIncrement.m_spin = spin;
				m_rotateSpinner.connect(spin);
				gtk_widget_show(GTK_WIDGET(spin));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(spin), 1, 2, 4, 5,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(spin), 60, -2);
				gtk_spin_button_set_wrap(spin, TRUE);
			}
			{
				GtkWidget* label = gtk_label_new ("Step");
				gtk_widget_show (label);
				gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(entry), 3, 4, 4, 5,
				                 (GtkAttachOptions) (GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				gtk_widget_set_usize(GTK_WIDGET(entry), 50, -2);
				m_rotateIncrement.m_entry = entry;
				m_rotateEntry.connect(entry);
			}
			{
				// match grid button
				GtkWidget* button = gtk_button_new_with_label ("Match Grid");
				gtk_widget_show (button);
				gtk_table_attach(GTK_TABLE(table), button, 2, 4, 5, 6,
				                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				                 (GtkAttachOptions) (0), 0, 0);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(OnBtnMatchGrid), 0);
			}
		}

		{
			GtkWidget* frame = gtk_frame_new ("Texturing");
			gtk_widget_show (frame);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), FALSE, FALSE, 0);
			{
				GtkWidget* table = gtk_table_new (4, 4, FALSE);
				gtk_widget_show (table);
				gtk_container_add (GTK_CONTAINER (frame), table);
				gtk_table_set_row_spacings(GTK_TABLE(table), 5);
				gtk_table_set_col_spacings(GTK_TABLE(table), 5);
				gtk_container_set_border_width (GTK_CONTAINER (table), 5);
				{
					GtkWidget* label = gtk_label_new ("Brush");
					gtk_widget_show (label);
					gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new ("Width");
					gtk_widget_show (label);
					gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* label = gtk_label_new ("Height");
					gtk_widget_show (label);
					gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
				}
				{
					GtkWidget* button = gtk_button_new_with_label ("Axial");
					gtk_widget_show (button);
					gtk_table_attach(GTK_TABLE(table), button, 0, 1, 1, 2,
					                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					g_signal_connect(G_OBJECT(button), "clicked",
					                 G_CALLBACK(OnBtnAxial), 0);
					gtk_widget_set_usize (button, 60, -2);
				}
				{
					GtkWidget* button = gtk_button_new_with_label ("Fit");
					gtk_widget_show (button);
					gtk_table_attach(GTK_TABLE(table), button, 1, 2, 1, 2,
					                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					g_signal_connect(G_OBJECT(button), "clicked",
					                 G_CALLBACK(OnBtnFaceFit), 0);
					gtk_widget_set_usize (button, 60, -2);
				}
				{
					GtkWidget* spin = gtk_spin_button_new (GTK_ADJUSTMENT (gtk_adjustment_new (1, 0, 1 << 16, 1, 10, 10)), 0, 6);
					gtk_widget_show (spin);
					gtk_table_attach(GTK_TABLE(table), spin, 2, 3, 1, 2,
					                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_widget_set_usize (spin, 60, -2);
					AddDialogData(*GTK_SPIN_BUTTON(spin), m_fitHorizontal);
				}
				{
					GtkWidget* spin = gtk_spin_button_new (GTK_ADJUSTMENT (gtk_adjustment_new (1, 0, 1 << 16, 1, 10, 10)), 0, 6);
					gtk_widget_show (spin);
					gtk_table_attach(GTK_TABLE(table), spin, 3, 4, 1, 2,
					                 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_widget_set_usize (spin, 60, -2);
					AddDialogData(*GTK_SPIN_BUTTON(spin), m_fitVertical);
				}
			}
		}
		if (!string_empty(g_pGameDescription->getKeyValue("si_flags"))) {
			{
				GtkFrame* frame = GTK_FRAME(gtk_frame_new("Surface Flags"));
				gtk_widget_show(GTK_WIDGET(frame));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
				{
					GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
					//gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
					gtk_widget_show(GTK_WIDGET(vbox3));
					gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox3));
					{
						GtkTable* table = GTK_TABLE(gtk_table_new(8, 4, FALSE));
						gtk_widget_show(GTK_WIDGET(table));
						gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(table), TRUE, TRUE, 0);
						gtk_table_set_row_spacings(table, 0);
						gtk_table_set_col_spacings(table, 0);

						GtkCheckButton** p = m_surfaceFlags;

						for (int c = 0; c != 4; ++c) {
							for (int r = 0; r != 8; ++r) {
								GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(getSurfaceFlagName(c * 8 + r)));
								gtk_widget_show(GTK_WIDGET(check));
								gtk_table_attach(table, GTK_WIDGET(check), c, c + 1, r, r + 1,
								                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
								                 (GtkAttachOptions)(0), 0, 0);
								*p++ = check;
								guint handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check), ApplyFlagsCaller(*this));
								g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
							}
						}
					}
				}
			}
			{
				GtkFrame* frame = GTK_FRAME(gtk_frame_new("Content Flags"));
				gtk_widget_show(GTK_WIDGET(frame));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
				{
					GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
					//gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
					gtk_widget_show(GTK_WIDGET(vbox3));
					gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox3));
					{

						GtkTable* table = GTK_TABLE(gtk_table_new(8, 4, FALSE));
						gtk_widget_show(GTK_WIDGET(table));
						gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(table), TRUE, TRUE, 0);
						gtk_table_set_row_spacings(table, 0);
						gtk_table_set_col_spacings(table, 0);

						GtkCheckButton** p = m_contentFlags;

						for (int c = 0; c != 4; ++c) {
							for (int r = 0; r != 8; ++r) {
								GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(getContentFlagName(c * 8 + r)));
								gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(check), FALSE);
								gtk_widget_show(GTK_WIDGET(check));
								gtk_table_attach(table, GTK_WIDGET(check), c, c + 1, r, r + 1,
								                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
								                 (GtkAttachOptions)(0), 0, 0);
								*p++ = check;
								guint handler_id = togglebutton_connect_toggled(GTK_TOGGLE_BUTTON(check), ApplyFlagsCaller(*this));
								g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler_id));
							}
						}

						// TODO: Why?
						// not allowed to modify detail flag using Surface Inspector
						gtk_widget_set_sensitive(GTK_WIDGET(m_contentFlags[BRUSH_DETAIL_FLAG]), FALSE);
					}
				}
			}
			{
				GtkFrame* frame = GTK_FRAME(gtk_frame_new("Value"));
				gtk_widget_show(GTK_WIDGET(frame));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
				{
					GtkVBox* vbox3 = GTK_VBOX(gtk_vbox_new(FALSE, 4));
					gtk_container_set_border_width(GTK_CONTAINER(vbox3), 4);
					gtk_widget_show(GTK_WIDGET(vbox3));
					gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox3));

					{
						GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
						gtk_widget_show(GTK_WIDGET(entry));
						gtk_box_pack_start(GTK_BOX(vbox3), GTK_WIDGET(entry), TRUE, TRUE, 0);
						m_valueEntryWidget = entry;
						m_valueEntry.connect(entry);
					}
				}
			}
		}
	}

	return window;
}

/*
==============
Update

Set the fields to the current texdef (i.e. map/texdef -> dialog widgets)
if faces selected (instead of brushes) -> will read this face texdef, else current texdef
===============
*/

void spin_button_set_value_no_signal(GtkSpinButton* spin, gdouble value) {
	guint handler_id = gpointer_to_int(g_object_get_data(G_OBJECT(spin), "handler"));
	g_signal_handler_block(G_OBJECT(gtk_spin_button_get_adjustment(spin)), handler_id);
	gtk_spin_button_set_value(spin, value);
	g_signal_handler_unblock(G_OBJECT(gtk_spin_button_get_adjustment(spin)), handler_id);
}

void spin_button_set_step_increment(GtkSpinButton* spin, gdouble value) {
	GtkAdjustment* adjust = gtk_spin_button_get_adjustment(spin);
	adjust->step_increment = value;
}

void SurfaceInspector::Update() {
	const char * name = SurfaceInspector_GetSelectedShader();

	if (shader_is_texture(name)) {
		gtk_entry_set_text(m_texture, shader_get_textureName(name));
	} else {
		gtk_entry_set_text(m_texture, "");
	}

	texdef_t shiftScaleRotate;

	ShiftScaleRotate_fromFace(shiftScaleRotate, SurfaceInspector_GetSelectedTexdef());

	// normalize again to hide the ridiculously high scale values that get created when using texlock
	shiftScaleRotate.shift[0] = float_mod(shiftScaleRotate.shift[0], (float)g_selectedShaderSize[0]);
	shiftScaleRotate.shift[1] = float_mod(shiftScaleRotate.shift[1], (float)g_selectedShaderSize[1]);

	{
		spin_button_set_value_no_signal(m_hshiftIncrement.m_spin, shiftScaleRotate.shift[0]);
		spin_button_set_step_increment(m_hshiftIncrement.m_spin, g_si_globals.shift[0]);
		entry_set_float(m_hshiftIncrement.m_entry, g_si_globals.shift[0]);
	}

	{
		spin_button_set_value_no_signal(m_vshiftIncrement.m_spin, shiftScaleRotate.shift[1]);
		spin_button_set_step_increment(m_vshiftIncrement.m_spin, g_si_globals.shift[1]);
		entry_set_float(m_vshiftIncrement.m_entry, g_si_globals.shift[1]);
	}

	{
		spin_button_set_value_no_signal(m_hscaleIncrement.m_spin, shiftScaleRotate.scale[0]);
		spin_button_set_step_increment(m_hscaleIncrement.m_spin, g_si_globals.scale[0]);
		entry_set_float(m_hscaleIncrement.m_entry, g_si_globals.scale[0]);
	}

	{
		spin_button_set_value_no_signal(m_vscaleIncrement.m_spin, shiftScaleRotate.scale[1]);
		spin_button_set_step_increment(m_vscaleIncrement.m_spin, g_si_globals.scale[1]);
		entry_set_float(m_vscaleIncrement.m_entry, g_si_globals.scale[1]);
	}

	{
		spin_button_set_value_no_signal(m_rotateIncrement.m_spin, shiftScaleRotate.rotate);
		spin_button_set_step_increment(m_rotateIncrement.m_spin, g_si_globals.rotate);
		entry_set_float(m_rotateIncrement.m_entry, g_si_globals.rotate);
	}

	if (!string_empty(g_pGameDescription->getKeyValue("si_flags"))) {
		ContentsFlagsValue flags(SurfaceInspector_GetSelectedFlags());

		entry_set_int(m_valueEntryWidget, flags.m_value);

		for (GtkCheckButton** p = m_surfaceFlags; p != m_surfaceFlags + 32; ++p) {
			toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(*p), flags.m_surfaceFlags & (1 << (p - m_surfaceFlags)));
		}

		for (GtkCheckButton** p = m_contentFlags; p != m_contentFlags + 32; ++p) {
			toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(*p), flags.m_contentFlags & (1 << (p - m_contentFlags)));
		}
	}
}

/**
 * @brief Reads the fields to get the current texdef (i.e. widgets -> MAP)
 */
void SurfaceInspector::ApplyShader() {
	StringOutputStream name(256);
	name << GlobalTexturePrefix_get() << gtk_entry_get_text(m_texture);

	// TTimo: detect and refuse invalid texture names (at least the ones with spaces)
	if (!texdef_name_valid(name.c_str())) {
		globalErrorStream() << "invalid texture name '" << name.c_str() << "'\n";
		SurfaceInspector_queueDraw();
		return;
	}

	UndoableCommand undo("textureNameSetSelected");
	Select_SetShader(name.c_str());
}

void SurfaceInspector::ApplyTexdef() {
	texdef_t shiftScaleRotate;

	shiftScaleRotate.shift[0] = static_cast<float>(gtk_spin_button_get_value_as_float(m_hshiftIncrement.m_spin));
	shiftScaleRotate.shift[1] = static_cast<float>(gtk_spin_button_get_value_as_float(m_vshiftIncrement.m_spin));
	shiftScaleRotate.scale[0] = static_cast<float>(gtk_spin_button_get_value_as_float(m_hscaleIncrement.m_spin));
	shiftScaleRotate.scale[1] = static_cast<float>(gtk_spin_button_get_value_as_float(m_vscaleIncrement.m_spin));
	shiftScaleRotate.rotate = static_cast<float>(gtk_spin_button_get_value_as_float(m_rotateIncrement.m_spin));

	TextureProjection projection;
	ShiftScaleRotate_toFace(shiftScaleRotate, projection);

	UndoableCommand undo("textureProjectionSetSelected");
	Select_SetTexdef(projection);
}

void SurfaceInspector::ApplyFlags() {
	unsigned int surfaceflags = 0;
	for (GtkCheckButton** p = m_surfaceFlags; p != m_surfaceFlags + 32; ++p) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*p))) {
			surfaceflags |= (1 << (p - m_surfaceFlags));
		}
	}

	unsigned int contentflags = 0;
	for (GtkCheckButton** p = m_contentFlags; p != m_contentFlags + 32; ++p) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*p))) {
			contentflags |= (1 << (p - m_contentFlags));
		}
	}

	int value = entry_get_int(m_valueEntryWidget);

	UndoableCommand undo("flagsSetSelected");
	/* set flags to the selection */
	Select_SetFlags(ContentsFlagsValue(surfaceflags, contentflags, value, true));
}


void Face_getTexture(Face& face, CopiedString& shader, TextureProjection& projection, ContentsFlagsValue& flags) {
	shader = face.GetShader();
	face.GetTexdef(projection);
	flags = face.getShader().m_flags;
}
typedef Function4<Face&, CopiedString&, TextureProjection&, ContentsFlagsValue&, void, Face_getTexture> FaceGetTexture;

void Face_setTexture(Face& face, const char* shader, const TextureProjection& projection, const ContentsFlagsValue& flags) {
	face.SetShader(shader);
	face.SetTexdef(projection);
	face.SetFlags(flags);
}
typedef Function4<Face&, const char*, const TextureProjection&, const ContentsFlagsValue&, void, Face_setTexture> FaceSetTexture;

typedef Callback3<CopiedString&, TextureProjection&, ContentsFlagsValue&> GetTextureCallback;
typedef Callback3<const char*, const TextureProjection&, const ContentsFlagsValue&> SetTextureCallback;

struct Texturable {
	GetTextureCallback getTexture;
	SetTextureCallback setTexture;
};


void Face_getClosest(Face& face, SelectionTest& test, SelectionIntersection& bestIntersection, Texturable& texturable) {
	SelectionIntersection intersection;
	face.testSelect(test, intersection);
	if (intersection.valid()
	        && SelectionIntersection_closer(intersection, bestIntersection)) {
		bestIntersection = intersection;
		texturable.setTexture = makeCallback3(FaceSetTexture(), face);
		texturable.getTexture = makeCallback3(FaceGetTexture(), face);
	}
}


class OccludeSelector : public Selector {
	SelectionIntersection& m_bestIntersection;
	bool& m_occluded;
public:
	OccludeSelector(SelectionIntersection& bestIntersection, bool& occluded) : m_bestIntersection(bestIntersection), m_occluded(occluded) {
		m_occluded = false;
	}
	void pushSelectable(Selectable& selectable) {
	}
	void popSelectable() {
	}
	void addIntersection(const SelectionIntersection& intersection) {
		if (SelectionIntersection_closer(intersection, m_bestIntersection)) {
			m_bestIntersection = intersection;
			m_occluded = true;
		}
	}
};

class BrushGetClosestFaceVisibleWalker : public scene::Graph::Walker {
	SelectionTest& m_test;
	Texturable& m_texturable;
	mutable SelectionIntersection m_bestIntersection;
public:
	BrushGetClosestFaceVisibleWalker(SelectionTest& test, Texturable& texturable) : m_test(test), m_texturable(texturable) {
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const {
		if (path.top().get().visible()) {
			BrushInstance* brush = Instance_getBrush(instance);
			if (brush != 0) {
				m_test.BeginMesh(brush->localToWorld());

				for (Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i) {
					Face_getClosest(*(*i), m_test, m_bestIntersection, m_texturable);
				}
			} else {
				SelectionTestable* selectionTestable = Instance_getSelectionTestable(instance);
				if (selectionTestable) {
					bool occluded;
					OccludeSelector selector(m_bestIntersection, occluded);
					selectionTestable->testSelect(selector, m_test);
					if (occluded)
						m_texturable = Texturable();
				}
			}
		}
		return true;
	}
};

Texturable Scene_getClosestTexturable(scene::Graph& graph, SelectionTest& test) {
	Texturable texturable;
	graph.traverse(BrushGetClosestFaceVisibleWalker(test, texturable));
	return texturable;
}

bool Scene_getClosestTexture(scene::Graph& graph, SelectionTest& test, CopiedString& shader, TextureProjection& projection, ContentsFlagsValue& flags) {
	Texturable texturable = Scene_getClosestTexturable(graph, test);
	if (texturable.getTexture != GetTextureCallback()) {
		texturable.getTexture(shader, projection, flags);
		return true;
	}
	return false;
}

void Scene_setClosestTexture(scene::Graph& graph, SelectionTest& test, const char* shader, const TextureProjection& projection, const ContentsFlagsValue& flags) {
	Texturable texturable = Scene_getClosestTexturable(graph, test);
	if (texturable.setTexture != SetTextureCallback()) {
		texturable.setTexture(shader, projection, flags);
	}
}


class FaceTexture {
public:
	TextureProjection m_projection;
	ContentsFlagsValue m_flags;
};

FaceTexture g_faceTextureClipboard;

void FaceTextureClipboard_setDefault() {
	g_faceTextureClipboard.m_flags = ContentsFlagsValue(0, 0, 0, false);
	TexDef_Construct_Default(g_faceTextureClipboard.m_projection);
}

void TextureClipboard_textureSelected(const char* shader) {
	FaceTextureClipboard_setDefault();
}

class TextureBrowser;
void TextureBrowser_SetSelectedShader(TextureBrowser& textureBrowser, const char* shader);
const char* TextureBrowser_GetSelectedShader(TextureBrowser& textureBrowser);

void Scene_copyClosestTexture(SelectionTest& test) {
	CopiedString shader;
	if (Scene_getClosestTexture(GlobalSceneGraph(), test, shader, g_faceTextureClipboard.m_projection, g_faceTextureClipboard.m_flags)) {
		TextureBrowser_SetSelectedShader(GlobalTextureBrowser(), shader.c_str());
	}
}

void Scene_applyClosestTexture(SelectionTest& test) {
	UndoableCommand command("facePaintTexture");

	Scene_setClosestTexture(GlobalSceneGraph(), test, TextureBrowser_GetSelectedShader(GlobalTextureBrowser()), g_faceTextureClipboard.m_projection, g_faceTextureClipboard.m_flags);

	SceneChangeNotify();
}

void SelectedFaces_copyTexture(void) {
	if (!g_SelectedFaceInstances.empty()) {
		Face& face = g_SelectedFaceInstances.last().getFace();
		face.GetTexdef(g_faceTextureClipboard.m_projection);
		g_faceTextureClipboard.m_flags = face.getShader().m_flags;

		TextureBrowser_SetSelectedShader(GlobalTextureBrowser(), face.getShader().getShader());
	}
}

void FaceInstance_pasteTexture(FaceInstance& faceInstance) {
	faceInstance.getFace().SetTexdef(g_faceTextureClipboard.m_projection);
	faceInstance.getFace().SetShader(TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
	faceInstance.getFace().SetFlags(g_faceTextureClipboard.m_flags);
	SceneChangeNotify();
}

bool SelectedFaces_empty() {
	return g_SelectedFaceInstances.empty();
}

void SelectedFaces_pasteTexture() {
	UndoableCommand command("facePasteTexture");
	g_SelectedFaceInstances.foreach(FaceInstance_pasteTexture);
}



void SurfaceInspector_constructPreferences(PreferencesPage& page) {
	page.appendCheckBox("", "Surface Inspector Increments Match Grid", g_si_globals.m_bSnapTToGrid);
}
void SurfaceInspector_constructPage(PreferenceGroup& group) {
	PreferencesPage page(group.createPage("Surface Inspector", "Surface Inspector Preferences"));
	SurfaceInspector_constructPreferences(page);
}
void SurfaceInspector_registerPreferencesPage() {
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, SurfaceInspector_constructPage>());
}

void SurfaceInspector_registerCommands() {
	GlobalCommands_insert("FitTexture", FreeCaller<SurfaceInspector_FitTexture>(), Accelerator('B', (GdkModifierType)GDK_SHIFT_MASK));
	GlobalCommands_insert("SurfaceInspector", FreeCaller<SurfaceInspector_toggleShown>(), Accelerator('S'));

	GlobalCommands_insert("FaceCopyTexture", FreeCaller<SelectedFaces_copyTexture>());
	GlobalCommands_insert("FacePasteTexture", FreeCaller<SelectedFaces_pasteTexture>());
}


#include "preferencesystem.h"


void SurfaceInspector_Construct() {
	g_SurfaceInspector = new SurfaceInspector;

	SurfaceInspector_registerCommands();

	FaceTextureClipboard_setDefault();

	GlobalPreferenceSystem().registerPreference("SurfaceWnd", getSurfaceInspector().m_importPosition, getSurfaceInspector().m_exportPosition);
	GlobalPreferenceSystem().registerPreference("SI_SurfaceTexdef_Scale1", FloatImportStringCaller(g_si_globals.scale[0]), FloatExportStringCaller(g_si_globals.scale[0]));
	GlobalPreferenceSystem().registerPreference("SI_SurfaceTexdef_Scale2", FloatImportStringCaller(g_si_globals.scale[1]), FloatExportStringCaller(g_si_globals.scale[1]));
	GlobalPreferenceSystem().registerPreference("SI_SurfaceTexdef_Shift1", FloatImportStringCaller(g_si_globals.shift[0]), FloatExportStringCaller(g_si_globals.shift[0]));
	GlobalPreferenceSystem().registerPreference("SI_SurfaceTexdef_Shift2", FloatImportStringCaller(g_si_globals.shift[1]), FloatExportStringCaller(g_si_globals.shift[1]));
	GlobalPreferenceSystem().registerPreference("SI_SurfaceTexdef_Rotate", FloatImportStringCaller(g_si_globals.rotate), FloatExportStringCaller(g_si_globals.rotate));
	GlobalPreferenceSystem().registerPreference("SnapTToGrid", BoolImportStringCaller(g_si_globals.m_bSnapTToGrid), BoolExportStringCaller(g_si_globals.m_bSnapTToGrid));

	typedef FreeCaller1<const Selectable&, SurfaceInspector_SelectionChanged> SurfaceInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(SurfaceInspectorSelectionChangedCaller());
	typedef FreeCaller<SurfaceInspector_updateSelection> SurfaceInspectorUpdateSelectionCaller;
	Brush_addTextureChangedCallback(SurfaceInspectorUpdateSelectionCaller());

	SurfaceInspector_registerPreferencesPage();
}
void SurfaceInspector_Destroy() {
	delete g_SurfaceInspector;
}

