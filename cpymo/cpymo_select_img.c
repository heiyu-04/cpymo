#include "cpymo_select_img.h"
#include "cpymo_engine.h"
#include <memory.h>
#include <assert.h>

void cpymo_select_img_reset(cpymo_select_img *img)
{
	if (img->select_img_image) {
		// select_img
		cpymo_backend_image_free(img->select_img_image);
	}
	else if(img->selections) {
		// select_imgs
		for (size_t i = 0; i < img->all_selections; ++i)
			if (img->selections[i].image)
				cpymo_backend_image_free(img->selections[i].image);
	}

	if (img->selections)
		free(img->selections);

	img->selections = NULL;
	img->select_img_image = NULL;
}

error_t cpymo_select_img_configuare_begin(
	struct cpymo_engine *engine, size_t selections,
	cpymo_parser_stream_span image_name_or_empty_when_select_imgs)
{
	engine->select_img.selections = 
		(cpymo_select_img_selection *)malloc(sizeof(cpymo_select_img_selection) * selections);
	if (engine->select_img.selections == NULL) return CPYMO_ERR_SUCC;

	memset(engine->select_img.selections, 0, sizeof(cpymo_select_img_selection) * selections);

	if (image_name_or_empty_when_select_imgs.len > 0) {
		error_t err = cpymo_assetloader_load_system_image(
			&engine->select_img.select_img_image,
			&engine->select_img.select_img_image_w, &engine->select_img.select_img_image_h,
			image_name_or_empty_when_select_imgs,
			"png",
			&engine->assetloader,
			cpymo_gameconfig_is_symbian(&engine->gameconfig));

		if (err != CPYMO_ERR_SUCC) {
			free(engine->select_img.selections);
			engine->select_img.selections = NULL;
			return err;
		}
	}

	engine->select_img.current_selection = 0;
	engine->select_img.all_selections = selections;

	return CPYMO_ERR_SUCC;
}

void cpymo_select_img_configuare_select_img_selection(cpymo_engine *e, float x, float y, bool enabled)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.select_img_image);

	cpymo_select_img_selection *sel = &e->select_img.selections[e->select_img.current_selection++];
	sel->image = e->select_img.select_img_image;

	sel->x = x;
	sel->y = y;
	sel->w = e->select_img.select_img_image_w / 2;
	sel->h = e->select_img.select_img_image_h / (int)e->select_img.all_selections;

	sel->enabled = enabled;
}

error_t cpymo_select_img_configuare_select_imgs_selection(cpymo_engine *e, cpymo_parser_stream_span image_name, float x, float y, bool enabled)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.select_img_image == NULL);

	cpymo_select_img_selection *sel = &e->select_img.selections[e->select_img.current_selection++];

	error_t err = cpymo_assetloader_load_system_image(
		&sel->image,
		&sel->w,
		&sel->h,
		image_name,
		"png",
		&e->assetloader,
		cpymo_gameconfig_is_symbian(&e->gameconfig)
	);

	if (err != CPYMO_ERR_SUCC) {
		return err;
	}

	sel->x = x;
	sel->y = y;
	sel->w /= 2;
	sel->enabled = enabled;

	return CPYMO_ERR_SUCC;
}

static bool cpymo_select_img_wait(struct cpymo_engine *e, float dt)
{
	return e->select_img.selections == NULL;
}

static bool cpymo_select_img_mouse_in_selection(cpymo_select_img *o, int sel, const cpymo_engine *e) {
	assert(sel >= 0 && sel < (int)o->all_selections);

	if (!e->input.mouse_position_useable) return false;
	cpymo_select_img_selection *s = &o->selections[sel];
	if (s->image) {
		float left = s->x - (float)s->w / 2.0f;
		float top = s->y - (float)s->h / 2.0f;
		float right = s->x + (float)s->w / 2.0f;
		float bottom = s->y + (float)s->h / 2.0f;

		float x = e->input.mouse_x;
		float y = e->input.mouse_y;

		return x >= left && x <= right && y >= top && y <= bottom;
	}

	return false;
}


void cpymo_select_img_configuare_end(struct cpymo_engine *e, int init_position)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.current_selection == e->select_img.all_selections);

	e->select_img.current_selection = init_position >= 0 ? init_position : 0;
	e->select_img.save_enabled = init_position == -1;

	// In pymo, if all options are disabled, it will enable every option.
	bool all_is_disabled = true;
	for (size_t i = 0; i < e->select_img.all_selections; ++i) {
		if (e->select_img.selections[i].enabled) {
			all_is_disabled = false;
			break;
		}
	}

	if (all_is_disabled)
		for (size_t i = 0; i < e->select_img.all_selections; ++i)
			e->select_img.selections[i].enabled = true;

	// Trim disabled images.
	for (size_t i = 0; i < e->select_img.all_selections; ++i) {
		if (!e->select_img.selections[i].enabled) {
			if (e->select_img.selections[i].image) {
				if (e->select_img.select_img_image == NULL)
					cpymo_backend_image_free(e->select_img.selections[i].image);
				e->select_img.selections[i].image = NULL;
			}
		}
	}

	for (int i = 0; i < (int)e->select_img.all_selections; ++i) {
		if (cpymo_select_img_mouse_in_selection(&e->select_img, i, e)) {
			if (i != e->select_img.current_selection) {
				e->select_img.current_selection = i;
				break;
			}
		}
	}

	cpymo_wait_register(&e->wait, &cpymo_select_img_wait);
	cpymo_engine_request_redraw(e);
}

static void cpymo_select_img_move(cpymo_select_img *o, int move) {
	assert(move == 1 || move == -1);
	
	o->current_selection += move;
	while (o->current_selection < 0) o->current_selection += (int)o->all_selections;
	while (o->current_selection >= (int)o->all_selections) o->current_selection -= (int)o->all_selections;

	if (o->selections[o->current_selection].image == NULL)
		cpymo_select_img_move(o, move);
}

static error_t cpymo_select_img_ok(cpymo_engine *e, int sel)
{
	const char *out_var = cpymo_gameconfig_is_mo1(&e->gameconfig) ? "F91" : "FSEL";

	error_t err = cpymo_vars_set(
		&e->vars,
		cpymo_parser_stream_span_pure(out_var),
		sel);
	if (err != CPYMO_ERR_SUCC) return err;

	cpymo_select_img_reset(&e->select_img);
	cpymo_engine_request_redraw(e);

	return err;
}

error_t cpymo_select_img_update(cpymo_engine *e)
{
	cpymo_select_img *o = &e->select_img;

	if (o->selections) {
		if (CPYMO_INPUT_JUST_PRESSED(e, down)) {
			cpymo_select_img_move(o, 1);
			cpymo_engine_request_redraw(e);
		}

		if (CPYMO_INPUT_JUST_PRESSED(e, up)) {
			cpymo_select_img_move(o, -1);
			cpymo_engine_request_redraw(e);
		}

		if (cpymo_input_mouse_moved(e) && e->input.mouse_position_useable) {
			for (int i = 0; i < (int)o->all_selections; ++i) {
				if (cpymo_select_img_mouse_in_selection(o, i, e)) {
					if (i != o->current_selection) {
						o->current_selection = i;
						cpymo_engine_request_redraw(e);
					}
				}
			}
		}

		if (CPYMO_INPUT_JUST_PRESSED(e, ok)) {
			return cpymo_select_img_ok(e, o->current_selection);
		}

		if (CPYMO_INPUT_JUST_PRESSED(e, cancel)) {
			if(o->selections[o->all_selections-1].image)
				return cpymo_select_img_ok(e, (int)o->all_selections - 1);
		}

		if (CPYMO_INPUT_JUST_PRESSED(e, mouse_button)) {
			for (int i = 0; i < (int)o->all_selections; ++i) {
				if (cpymo_select_img_mouse_in_selection(o, i, e)) {
					o->current_selection = i;
					return cpymo_select_img_ok(e, i);
				}
			}
		}
	}

	return CPYMO_ERR_SUCC;
}

void cpymo_select_img_draw(const cpymo_select_img *o)
{
	if (o->selections) {
		for (int i = 0; i < (int)o->all_selections; ++i) {
			const cpymo_select_img_selection *sel = &o->selections[i];
			if (sel->image) {
				const bool selected = o->current_selection == i;
				cpymo_backend_image_draw(
					sel->x - (float)sel->w / 2.0f,
					sel->y - (float)sel->h / 2.0f,
					(float)sel->w,
					(float)sel->h,
					sel->image,
					selected ? sel->w : 0,
					o->select_img_image ? i * sel->h : 0,
					sel->w,
					sel->h,
					1.0f,
					cpymo_backend_image_draw_type_sel_img
				);
			}
		}
	}
}