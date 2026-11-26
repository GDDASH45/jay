/* SPDX-License-Identifier: GPL-2.0-only */
#include "jay_internal.h"

Function functions[MAX_FUNCTIONS];
size_t function_count;

Function *find_function(const char *name)
{
    for (size_t i = 0; i < function_count; i++) {
        if (strcmp(functions[i].name, name) == 0)
            return &functions[i];
    }

    return NULL;
}

bool define_function(const char *name, Parameter *parameters,
                     size_t parameter_count, size_t body_start,
                     size_t body_end, ValueType return_type, int line)
{
    if (find_function(name)) {
        fail_at(line, "function is already declared");
        return false;
    }

    if (function_count >= MAX_FUNCTIONS) {
        fail_at(line, "function limit reached");
        return false;
    }

    if (parameter_count > MAX_PARAMETERS) {
        fail_at(line, "too many function parameters");
        return false;
    }

    Function *function = &functions[function_count++];

    snprintf(function->name, sizeof(function->name), "%s", name);
    memcpy(function->parameters, parameters,
           parameter_count * sizeof(Parameter));

    function->parameter_count = parameter_count;
    function->body_start = body_start;
    function->body_end = body_end;
    function->line = line;
    function->return_type = return_type;
    snprintf(function->module_name, sizeof(function->module_name), "%s", active_module);

    return true;
}

Value call_function(const char *name, Value *arguments,
                    size_t argument_count, int line)
{
    Value library_result = make_void();

    if (jaystdlib_call(name, arguments, argument_count,
                       &library_result, line))
        return library_result;

    Function *function = find_function(name);

    if (!function) {
        char message[256];
        snprintf(message, sizeof(message),
                 "undefined function '%s'", name);
        fail_at(line, message);
        return make_void();
    }

    if (argument_count != function->parameter_count) {
        fail_at(line, "incorrect number of function arguments");
        return make_void();
    }

    if (scope_depth + 1 >= MAX_SCOPE_DEPTH) {
        fail_at(line, "maximum function call depth reached");
        return make_void();
    }

    size_t saved_variable_count = variable_count;
    size_t saved_current = current;
    size_t saved_scope_depth = scope_depth;
    int saved_flow_state = flow_state;

    Value saved_return = clone_value(return_value);

    scope_depth++;
    scope_starts[scope_depth] = variable_count;

    flow_state = 0;
    free_value(&return_value);
    return_value = make_void();

    bool valid = true;

    for (size_t i = 0; i < argument_count; i++) {
        Parameter *parameter = &function->parameters[i];
        Value value = arguments[i];
        Value converted = make_void();
        bool owns_converted = false;

        if (parameter->typed) {
            if (!convert_value(value, parameter->type, &converted)) {
                fail_at(line, "function argument has an incompatible type");
                valid = false;
                break;
            }

            value = converted;
            owns_converted = true;
        }

        if (!define_variable(parameter->name, value,
                             parameter->typed, parameter->type,
                             false, line)) {
            valid = false;
        }

        if (owns_converted)
            free_value(&converted);

        if (!valid)
            break;
    }

    if (valid)
        execute_range(function->body_start, function->body_end);

    Value result = make_void();

    if (valid && flow_state == 3) {
        if (function->return_type == V_INFER) {
            result = clone_value(return_value);
        } else if (return_value.type == V_VOID || function->return_type == V_VOID) {
            if (return_value.type != function->return_type)
                fail_at(line, "function return value does not match declared return type");
        } else if (!convert_value(return_value, function->return_type, &result)) {
            fail_at(line, "function returned a value with the wrong type");
        }
    } else if (valid && function->return_type != V_VOID &&
               function->return_type != V_INFER) {
        fail_at(line, "non-void function did not return a value");
    }

    for (size_t i = saved_variable_count; i < variable_count; i++)
        free_value(&variables[i].value);

    variable_count = saved_variable_count;
    scope_depth = saved_scope_depth;
    current = saved_current;
    flow_state = saved_flow_state;

    free_value(&return_value);
    return_value = saved_return;

    return result;
}