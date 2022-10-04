
#include <yaml.h>
#include <jansson.h>

#include "y2j.h"

#ifdef Y2J_TESTER
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#endif

static json_t *_y2j_parse_scalar(yaml_event_t);
static json_t *_y2j_parse_sequence(yaml_parser_t*);
static json_t *_y2j_parse_mapping(yaml_parser_t*);
static json_t *_y2j_parse_document(yaml_parser_t*);
static json_t *_y2j_parse_stream(yaml_parser_t*);

static json_t *
_y2j_parse_scalar(yaml_event_t ev)
{
	json_t *value;
	// check quoted
	if(
		ev.data.scalar.quoted_implicit
		||
		ev.data.scalar.style==YAML_DOUBLE_QUOTED_SCALAR_STYLE
		||
		ev.data.scalar.style==YAML_SINGLE_QUOTED_SCALAR_STYLE
		) {
		// it's a string
		value = json_stringn(ev.data.scalar.value, ev.data.scalar.length);
		// if(!value) {
		// 	//fprintf(stderr, "failed to create string\n");
		// }
		return value;
	}
	// otherwise, the type is implicit

	char *endptr;
	char *begptr = (char*)ev.data.scalar.value;
	// try integer (implicit base)
	{
		long lval = strtol(begptr, &endptr, 0);
		if((endptr - begptr) == ev.data.scalar.length) {
			// parsed the whole string
			value = json_integer(lval);
			// if(!value) {
			// 	//fprintf(stderr, "failed to create integer\n");
			// }
			return value;
		}
	}

	// TODO integer with base16 or base60?

	// try double/real
	{
		double dval = strtod(begptr, &endptr);
		if((endptr-begptr)==ev.data.scalar.length) {
			value = json_real(dval);
			// if(!value)
			// 	//fprintf(stderr, "failed to create real\n");
			return value;
		}
	}

	// when everything else fails,
	value = json_stringn(ev.data.scalar.value, ev.data.scalar.length);
	// if(!value)
	// 	//fprintf(stderr, "failed to create string\n");
	return value;
}

static json_t *
_y2j_parse_sequence(yaml_parser_t *parser)
{
	json_t *seq = json_array();
	if(!seq) {
		//fprintf(stderr, "failed to create json array\n");
		return NULL;
	}
	yaml_event_t ev;
	while(1) {
		json_t *item = NULL;
break_2:
		if(!yaml_parser_parse(parser, &ev)) {
			//fprintf(stderr, "error parsing\n");
			json_decref(seq);
			return NULL;
		}
		switch(ev.type) {
		case YAML_SEQUENCE_END_EVENT:
			return seq;
		case YAML_ALIAS_EVENT:
			//fprintf(stderr, "no idea how to handle ALIAS\n");
			goto break_2;
		case YAML_SCALAR_EVENT:
			item = _y2j_parse_scalar(ev);
			break;
		case YAML_SEQUENCE_START_EVENT:
			item = _y2j_parse_sequence(parser);
			break;
		case YAML_MAPPING_START_EVENT:
			item = _y2j_parse_mapping(parser);
			break;
		}
		if(!item) {
			json_decref(seq);
			return NULL;
		}
		// else
		if(json_array_append_new(seq, item)) {
			//fprintf(stderr, "failed to append to array\n");
			json_decref(seq);
			json_decref(item);
			return NULL;
		}
	}

	// not reached
	return seq;
}

static json_t *
_y2j_parse_mapping(yaml_parser_t *parser)
{
	json_t *map = json_object();
	if(!map) {
		//fprintf(stderr, "failed to create json object\n");
		return NULL;
	}

	yaml_event_t ev_k, ev_v;
	while(1) {
break_2:
		// key: node
		json_t *value = NULL;

		// key
		if(!yaml_parser_parse(parser, &ev_k)) {
			//fprintf(stderr, "error parsing\n");
			json_decref(map);
			return NULL;
		}
		if(ev_k.type == YAML_MAPPING_END_EVENT)
			break;
		if(ev_k.type != YAML_SCALAR_EVENT) {
			//fprintf(stderr, "wrong key event: %d\n", (int)ev_k.type);
			json_decref(map);
			return NULL;
		}

		// value
		if(!yaml_parser_parse(parser, &ev_v)) {
			//fprintf(stderr, "error parsing\n");
			json_decref(map);
			return NULL;
		}

		if(ev_v.type == YAML_ALIAS_EVENT) {
			// ignore it
			//fprintf(stderr, "no idea how to parse ALIAS\n");
			continue;
		}

		switch(ev_v.type) {
		case YAML_SCALAR_EVENT:
			value = _y2j_parse_scalar(ev_v);
			break;
		case YAML_SEQUENCE_START_EVENT:
			value = _y2j_parse_sequence(parser);
			break;
		case YAML_MAPPING_START_EVENT:
			value = _y2j_parse_mapping(parser);
			break;
		default:
			//fprintf(stderr, "unexpected event: %d\n", (int)ev_v.type);
			json_decref(map);
			return NULL;
		}
		if(!value) {
			json_decref(map);
			return NULL;
		}
		if(json_object_set_new(map, ev_k.data.scalar.value, value)) {
			//fprintf(stderr, "failed to set value on object\n");
			json_decref(value);
			json_decref(map);
			return NULL;
		}
	}

	return map;
}

static json_t *
_y2j_parse_document(yaml_parser_t *parser)
{
	yaml_event_t event;
	json_t *document = NULL;
	int done = 0;
	// document is a single NODE
	while(1) {
		if(!yaml_parser_parse(parser, &event)) {
			//fprintf(stderr, "error parsing\n");
			return NULL;
		}
		if(done && event.type != YAML_DOCUMENT_END_EVENT) {
			//fprintf(stderr, "document has too many nodes\n");
			json_decref(document);
			return NULL;
		}
		if(event.type == YAML_DOCUMENT_END_EVENT)
			// TODO should check for null?
			break;

		switch(event.type) {
		case YAML_SCALAR_EVENT:
			document = _y2j_parse_scalar(event);
			break;
		case YAML_SEQUENCE_START_EVENT:
			document = _y2j_parse_sequence(parser);
			break;
		case YAML_MAPPING_START_EVENT:
			document = _y2j_parse_mapping(parser);
			break;
		case YAML_ALIAS_EVENT:
			//fprintf(stderr, "no idea how to handle ALIAS\n");
			break;
		// no default
		}
		if(!document) {
			return NULL;
		}
		done ++;
	}

	return document;
}

static json_t *
_y2j_parse_stream(yaml_parser_t *parser)
{
	yaml_event_t event;
	json_t *stream = json_array();
	if(!stream) {
		//fprintf(stderr, "failed to create JSON array\n");
		return NULL;
	}

	while(1) {
		if(!yaml_parser_parse(parser, &event)) {
			//fprintf(stderr, "error parsing\n");
			json_decref(stream);
			return NULL;
		}
		// expecting documents
		if(event.type == YAML_DOCUMENT_START_EVENT) {
			json_t *document = _y2j_parse_document(parser);
			if(!document) {
				// error
				json_decref(stream);
				return NULL;
			}
			if(json_array_append_new(stream, document)) {
				//fprintf(stderr, "failed to append document to stream\n");
				json_decref(document);
				json_decref(stream);
				return NULL;
			}
		} else if(event.type == YAML_STREAM_END_EVENT) {
			// that's it
			break;
		} else {
			// unexpected event
			//fprintf(stderr, "unexpected event: %d\n", (int)event.type);
			json_decref(stream);
			return NULL;
		}
	}

	return stream;
}

json_t *
y2j_parse(yaml_parser_t *parser)
{
    yaml_event_t ev;
    json_t *stream;

    // parse stream
    if(!yaml_parser_parse(parser, &ev)) {
        // parsing error
        return NULL;
    }
    if(ev.type != YAML_STREAM_START_EVENT) {
        // unexpected
        return NULL;
    }
    // else
    stream = _y2j_parse_stream(parser);
    if(!stream) {
        // error
        return NULL;
    }

    // if it's only one document, return it
    if(json_array_size(stream) == 1) {
        json_t *to_delete = stream;
        stream = json_array_get(stream, 0);
        if(!stream) {
            // weird
            return to_delete;
        }
        json_incref(stream);
        json_decref(to_delete);
    }

    return stream;
}

#ifdef Y2J_TESTER

json_t *parse_yml_file(char *file)
{
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    FILE* input = fopen(file, "rb");
    if(!input) {
        fprintf(stderr, "failed to open '%s'\n", file);
        yaml_parser_delete(&parser);
        return NULL;
    }
    yaml_parser_set_input_file(&parser, input);

    json_t *v = y2j_parse(&parser);

    yaml_parser_delete(&parser);
    fclose(input);
    return v;   // may be null
}

json_t *parse_json_file(char *file)
{
    FILE*input = fopen(file, "rb");
    if(!input) {
        fprintf(stderr, "failed to open '%s'\n", file);
        return NULL;
    }
    json_error_t jerror;
    json_t *v = json_loadf(input, 0, &jerror);
    // ignore `jerror`
    return v;
}

int main(int argc, char**argv)
{
    // iterate all "{NAME}.yml" files in tests/ folder
    // parse it and compare with "{NAME}.json"
    char path[280] = "tests/";
    // strlen("tests/") = 6
    char *name = path+6;
    char *ext;
    DIR *test_dir = opendir("tests");
    if(!test_dir) {
        fprintf(stderr, "failed to open 'tests/' folder\n");
        return 1;
    }
    struct dirent *dent;
    json_t *f_yaml, *f_json;
    while((dent = readdir(test_dir)) != NULL) {
        char *dot = strrchr(dent->d_name, '.');
        if(!dot)
            continue;
        if(strncmp(dot, ".yml", 3))
            // differs
            continue;
        fprintf(stderr, "checking '%s'\n", dent->d_name);
        int base_name_len = dot - dent->d_name;
        // first get fullpath
        strcpy(name, dent->d_name);
        // parse yaml
        f_yaml = parse_yml_file(path);
        if(!f_yaml) {
            fprintf(stderr, "failed to load yaml\n");
            continue;
        }
        // now change to json name
        memcpy(name+base_name_len, ".json", 6);
        f_json = parse_json_file(path);
        if(!f_json) {
            fprintf(stderr, "failed to load json\n");
            json_decref(f_yaml);
            continue;
        }

        if(!json_equal(f_yaml, f_json)) {
            fprintf(stderr, "objects differ\n");
            // print them
            fprintf(stderr, "-- FROM YAML --\n");
            json_dumpf(f_yaml, stderr, JSON_INDENT(2)|JSON_REAL_PRECISION(2));
            fprintf(stderr, "\n-- FROM JSON --\n");
            json_dumpf(f_json, stderr, JSON_INDENT(2)|JSON_REAL_PRECISION(2));
            fprintf(stderr, "\n---------------\n");
        } else {
            fprintf(stderr, "OK\n");
        }

        json_decref(f_yaml);
        json_decref(f_json);
        // continue
    }
}

#endif
