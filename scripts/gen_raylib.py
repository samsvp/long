"""
Generates the raylib module for long from raylib.h.

Usage: python3 scripts/gen_raylib.py /usr/local/include/raylib.h /usr/local/lib/libraylib.a > vendor/raylib/raylib.c

Structs become user values with a constructor named after the struct. Enum values and colors are
functions without arguments. Functions with pointer parameters or results (other than const char*),
callbacks or varargs are skipped, and so are functions the library does not define.
"""

import re
import subprocess
import sys

NUMBERS = {"int", "unsigned int", "float", "double", "long", "char", "unsigned char", "short",
           "unsigned short"}
TEXT = "const char*"
# Longest output of %g for a number, and of %p
NUMBER_LEN = 13
POINTER_LEN = 18


def norm(t):
    return re.sub(r"\s*\*", "*", t.strip())


class Field:
    def __init__(self, ctype, name, dim):
        self.ctype = ctype
        self.name = name
        self.dim = dim


class Struct:
    def __init__(self, name, fields):
        self.name = name
        self.fields = fields
        self.tag = "TT_" + name.upper()


def parse(src):
    aliases = {}
    pointer_types = set()
    for base, alias in re.findall(r"^typedef (\w+) (\w+);", src, re.M):
        aliases[alias] = base
    for alias in re.findall(r"^typedef \w+ \*(\w+);", src, re.M):
        pointer_types.add(alias)

    structs = {}
    for name, body in re.findall(r"typedef struct (\w+) \{(.*?)\} \w+;", src, re.S):
        fields = []
        for line in body.split("\n"):
            decl = line.split("//")[0].strip().rstrip(";")
            if not decl:
                continue
            parts = [p.strip() for p in decl.split(",")]
            m = re.match(r"^(.*?)\s*(\**)(\w+)(?:\[(\d+)\])?$", parts[0])
            base, stars = m.group(1), m.group(2)
            ctype = norm(base + " " + stars) if stars else base
            fields.append(Field(ctype, m.group(3), int(m.group(4)) if m.group(4) else None))
            for p in parts[1:]:
                fields.append(Field(ctype, p, None))
        structs[name] = Struct(name, fields)

    enums = []
    for body in re.findall(r"typedef enum \{(.*?)\} \w+;", src, re.S):
        for line in body.split("\n"):
            m = re.match(r"^\s*([A-Z_0-9]+)\s*(=|,|$)", line.split("//")[0])
            if m:
                enums.append(m.group(1))

    colors = re.findall(r"^#define (\w+)\s+CLITERAL\(Color\)", src, re.M)

    functions = []
    for ret, name, params in re.findall(r"^RLAPI (.+?)(\w+)\((.*)\);", src, re.M):
        ps = []
        if params.strip() not in ("", "void"):
            for p in params.split(","):
                p = p.strip()
                if p == "...":
                    ps.append(("...", "..."))
                    continue
                m = re.match(r"^(.*?)(\w+)(\[\d*\])?$", p)
                ps.append((norm(m.group(1) + ("*" if m.group(3) else "")), m.group(2)))
        functions.append((norm(ret), name, ps))

    return aliases, pointer_types, structs, enums, colors, functions


def defined_functions(lib):
    out = subprocess.run(["nm", "--defined-only", lib], capture_output=True, text=True, check=True).stdout
    names = [line.split()[-1] for line in out.splitlines() if re.search(r" T \w+$", line)]
    # macOS prefixes C symbols with an underscore
    return {n[1:] if n.startswith("_") else n for n in names}


class Gen:
    def __init__(self, src, defined):
        (self.aliases, self.pointer_types, self.structs, self.enums, self.colors,
         self.functions) = parse(src)
        self.defined = defined
        self.out = []

    def base(self, t):
        while t in self.aliases:
            t = self.aliases[t]
        return t

    def is_struct(self, t):
        return self.base(t) in self.structs

    def kind(self, f):
        if "*" in f.ctype or f.ctype in self.pointer_types:
            return "ptr"
        if f.dim is not None:
            if f.ctype == "char":
                return "str"
            return "structarr" if self.is_struct(f.ctype) else "numarr"
        if f.ctype == "bool":
            return "bool"
        if f.ctype in NUMBERS:
            return "num"
        assert self.is_struct(f.ctype), f.ctype
        return "struct"

    def stored(self, s):
        return [f for f in s.fields if self.kind(f) in ("struct", "str", "numarr", "structarr")]

    def payload(self, s):
        return s.name + "_value" if self.stored(s) else s.name

    def fmt_len(self, t):
        s = self.structs[self.base(t)]
        n = len(s.name) + 5
        for f in s.fields:
            k = self.kind(f)
            n += len(f.name) + 4
            if k == "num":
                n += NUMBER_LEN
            elif k == "bool":
                n += 5
            elif k == "ptr":
                n += POINTER_LEN
            elif k == "str":
                n += f.dim + 2
            elif k == "numarr":
                n += f.dim * (NUMBER_LEN + 2) + 2
            elif k == "structarr":
                n += f.dim * (self.fmt_len(f.ctype) + 2) + 2
            else:
                n += self.fmt_len(f.ctype)
        return n

    def w(self, line=""):
        self.out.append(line)

    # Structs

    def gen_struct(self, s):
        name = s.name
        stored = self.stored(s)
        payload = self.payload(s)
        w = self.w

        if stored:
            w("typedef struct {")
            w(f"    {name} raw;")
            for f in stored:
                w(f"    value_t {f.name};")
            w(f"}} {payload};")
            w()

        # equality over the raylib struct
        w(f"static bool eq_{name}(const {name}* a, const {name}* b)")
        w("{")
        for f in s.fields:
            k = self.kind(f)
            if k in ("num", "bool", "ptr"):
                w(f"    if (a->{f.name} != b->{f.name})")
                w("        return false;")
            elif k == "struct":
                w(f"    if (!eq_{self.base(f.ctype)}(&a->{f.name}, &b->{f.name}))")
                w("        return false;")
            elif k == "str":
                w(f"    if (strncmp(a->{f.name}, b->{f.name}, {f.dim}) != 0)")
                w("        return false;")
            elif k == "numarr":
                w(f"    for (int i = 0; i < {f.dim}; i++)")
                w(f"        if (a->{f.name}[i] != b->{f.name}[i])")
                w("            return false;")
            else:
                w(f"    for (int i = 0; i < {f.dim}; i++)")
                w(f"        if (!eq_{self.base(f.ctype)}(&a->{f.name}[i], &b->{f.name}[i]))")
                w("            return false;")
        w("    return true;")
        w("}")
        w()

        w(f"static uint32_t hash_{name}(const {name}* v)")
        w("{")
        w("    uint32_t h = 0;")
        for f in s.fields:
            k = self.kind(f)
            if k in ("num", "bool"):
                w(f"    h += number_hash(v->{f.name});")
            elif k == "ptr":
                w(f"    h += (uint32_t)(uintptr_t)v->{f.name};")
            elif k == "struct":
                w(f"    h += hash_{self.base(f.ctype)}(&v->{f.name});")
            elif k == "str":
                w(f"    for (int i = 0; i < {f.dim} && v->{f.name}[i] != '\\0'; i++)")
                w(f"        h = h * 31 + (uint32_t)v->{f.name}[i];")
            elif k == "numarr":
                w(f"    for (int i = 0; i < {f.dim}; i++)")
                w(f"        h += number_hash(v->{f.name}[i]);")
            else:
                w(f"    for (int i = 0; i < {f.dim}; i++)")
                w(f"        h += hash_{self.base(f.ctype)}(&v->{f.name}[i]);")
        w("    return h;")
        w("}")
        w()

        # string representation, written like snprintf
        w(f"static int fmt_{name}(const {name}* v, char* buf, size_t size)")
        w("{")
        w(f'    int n = snprintf(buf, size, "{name} {{ ");')
        for i, f in enumerate(s.fields):
            sep = ", " if i + 1 < len(s.fields) else " }"
            k = self.kind(f)
            if k == "num":
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: %g{sep}", (double)v->{f.name});')
            elif k == "bool":
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: %s{sep}", v->{f.name} ? "true" : "false");')
            elif k == "ptr":
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: %p{sep}", (const void*)v->{f.name});')
            elif k == "str":
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: \\"%.{f.dim}s\\"{sep}", v->{f.name});')
            elif k == "struct":
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: ");')
                w(f"    n += fmt_{self.base(f.ctype)}(&v->{f.name}, at(buf, size, n), rest(size, n));")
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{sep}");')
            else:
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{f.name}: (");')
                w(f"    for (int i = 0; i < {f.dim}; i++) {{")
                if k == "numarr":
                    w(f'        n += snprintf(at(buf, size, n), rest(size, n), "%g", (double)v->{f.name}[i]);')
                else:
                    w(f"        n += fmt_{self.base(f.ctype)}(&v->{f.name}[i], at(buf, size, n), rest(size, n));")
                w(f'        n += snprintf(at(buf, size, n), rest(size, n), i + 1 < {f.dim} ? ", " : ")");')
                w("    }")
                w(f'    n += snprintf(at(buf, size, n), rest(size, n), "{sep}");')
        w("    return n;")
        w("}")
        w()

        raw = "&v->raw" if stored else "v"
        w(f"static bool {name}_eql(void* a, void* b)")
        w("{")
        if stored:
            w(f"    return eq_{name}(&(({payload}*)a)->raw, &(({payload}*)b)->raw);")
        else:
            w(f"    return eq_{name}(a, b);")
        w("}")
        w()
        w(f"static uint32_t {name}_hash(void* p)")
        w("{")
        w(f"    const {payload}* v = p;")
        w(f"    return hash_{name}({raw});")
        w("}")
        w()
        w(f"static void {name}_free(void* p, const sv_allocator_t* a)")
        w("{")
        if stored:
            w(f"    {payload}* v = p;")
            for f in stored:
                w(f"    long_value_free(&v->{f.name}, a);")
        w("    long_free(a, p);")
        w("}")
        w()

        w(f"static bool {name}_field(void* p, uint32_t index, value_t* out, const vm_ctx_t* ctx)")
        w("{")
        w(f"    const {payload}* v = p;")
        w("    const char* name = long_field_name(ctx, index);")
        rv = "v->raw." if stored else "v->"
        first = True
        for f in s.fields:
            k = self.kind(f)
            if k == "num":
                expr = f"long_value_number({rv}{f.name})"
            elif k == "bool":
                expr = f"long_value_bool({rv}{f.name})"
            elif k == "ptr":
                expr = f"long_value_c((void*){rv}{f.name})"
            else:
                expr = f"v->{f.name}"
            w(f'    {"if" if first else "else if"} (strcmp(name, "{f.name}") == 0) *out = {expr};')
            first = False
        w("    else return false;")
        w("    return true;")
        w("}")
        w()

        w(f"static value_t {name}_string(void* p, const vm_ctx_t* ctx)")
        w("{")
        w(f"    const {payload}* v = p;")
        w(f"    char buf[{self.fmt_len(name) + 1}];")
        w(f"    return to_string(buf, fmt_{name}({raw}, buf, sizeof(buf)), ctx);")
        w("}")
        w()

        w(f"static const user_value_vtable_t {name}_vtable = {{")
        w(f"    .eql_fn = {name}_eql,")
        w(f"    .free_fn = {name}_free,")
        w(f"    .get_field_fn = {name}_field,")
        w(f"    .to_string_fn = {name}_string,")
        w(f"    .hash_fn = {name}_hash,")
        w("};")
        w()

        w(f"static inline value_t wrap_{name}({name} v, const sv_allocator_t* a)")
        w("{")
        w(f"    {payload}* p = long_alloc(a, sizeof(*p));")
        w("    if (p == NULL)")
        w("        return long_value_oom();")
        if stored:
            w("    p->raw = v;")
            for f in stored:
                k = self.kind(f)
                if k == "struct":
                    w(f"    p->{f.name} = wrap_{self.base(f.ctype)}(v.{f.name}, a);")
                elif k == "str":
                    w("    {")
                    w(f"        char text[{f.dim + 1}];")
                    w(f'        snprintf(text, sizeof(text), "%.{f.dim}s", v.{f.name});')
                    w(f"        p->{f.name} = long_value_str(text, a);")
                    w("    }")
                elif k == "numarr":
                    w("    {")
                    w(f"        value_t items[{f.dim}];")
                    w(f"        for (int i = 0; i < {f.dim}; i++)")
                    w(f"            items[i] = long_value_number(v.{f.name}[i]);")
                    w(f"        p->{f.name} = long_value_tuple(items, {f.dim}, a);")
                    w("    }")
                else:
                    w("    {")
                    w(f"        value_t items[{f.dim}];")
                    w(f"        for (int i = 0; i < {f.dim}; i++)")
                    w(f"            items[i] = wrap_{self.base(f.ctype)}(v.{f.name}[i], a);")
                    w(f"        p->{f.name} = long_value_tuple(items, {f.dim}, a);")
                    w(f"        for (int i = 0; i < {f.dim}; i++) {{")
                    w("            if (long_is_oom(items[i]))")
                    w(f"                long_value_free(&p->{f.name}, a);")
                    w("            long_value_free(&items[i], a);")
                    w("        }")
                    w("    }")
            w(f"    if ({' || '.join(f'long_is_oom(p->{f.name})' for f in stored)}) {{")
            w(f"        {name}_free(p, a);")
            w("        return long_value_oom();")
            w("    }")
        else:
            w("    *p = v;")
        w(f'    return long_value_user(p, &{name}_vtable, {s.tag}, "{name}", a);')
        w("}")
        w()

        if any(f.dim is not None for f in s.fields):
            return
        w(f"static value_t c_{name}(const value_t* args, uint8_t n, vm_t* vm)")
        w("{")
        w("    (void)n;")
        params = [(f.ctype, f.name) for f in s.fields]
        args = self.gen_checks(params)
        w(f"    {name} v = {{ {', '.join(args)} }};")
        w(f"    return wrap_{name}(v, long_allocator(vm));")
        w("}")
        w()

    # Functions

    def param_kind(self, t):
        if t == TEXT:
            return "text"
        if t == "bool":
            return "bool"
        if t in NUMBERS:
            return "num"
        if t in self.pointer_types or "*" in t:
            return "ptr"
        if self.is_struct(t):
            return "struct"
        return None

    def gen_checks(self, params):
        """Emits the type checks of the arguments, returning the C expression of each."""
        w = self.w
        kinds = [self.param_kind(t) for t, _ in params]
        i = 0
        while i < len(params):
            k = kinds[i]
            j = i
            while j + 1 < len(params) and kinds[j + 1] == k and k in ("num", "bool"):
                j += 1
            check, expected = {
                "num": ("long_is_number", "number"),
                "bool": ("long_is_bool", "bool"),
                "text": ("long_is_str", "string"),
                "ptr": ("long_is_c", "c_value"),
            }.get(k, (None, None))
            if k == "struct":
                base = self.base(params[i][0])
                w(f"    const {base}* a{i} = long_as_user(args[{i}], &{base}_vtable);")
                w(f"    if (a{i} == NULL)")
                w(f'        return long_type_error(args[{i}], "{base}", vm);')
            elif j > i:
                w(f"    for (int i = {i}; i <= {j}; i++)")
                w(f"        if (!{check}(args[i]))")
                w(f'            return long_type_error(args[i], "{expected}", vm);')
            else:
                w(f"    if (!{check}(args[{i}]))")
                w(f'        return long_type_error(args[{i}], "{expected}", vm);')
            i = j + 1

        exprs = []
        for i, (t, _) in enumerate(params):
            k = kinds[i]
            if k == "num":
                exprs.append(f"({t})long_as_number(args[{i}])")
            elif k == "bool":
                exprs.append(f"long_as_bool(args[{i}])")
            elif k == "ptr":
                exprs.append(f"long_as_c(args[{i}])")
            elif k == "text":
                exprs.append(f"a{i}")
            else:
                exprs.append(f"*a{i}")
        return exprs

    def supported(self, ret, params):
        if ret != "void" and self.param_kind(ret) not in ("num", "bool", "text", "struct"):
            return False
        return all(self.param_kind(t) in ("num", "bool", "text", "struct") for t, _ in params)

    def gen_function(self, ret, name, params):
        w = self.w
        w(f"static value_t f_{name}(const value_t* args, uint8_t n, vm_t* vm)")
        w("{")
        k = self.param_kind(ret)
        texts = [i for i, (t, _) in enumerate(params) if t == TEXT]
        alloc = "a" if texts else "long_allocator(vm)"
        if params:
            w("    (void)n;")
        else:
            w("    (void)args; (void)n;" if k in ("text", "struct") else "    (void)args; (void)n; (void)vm;")
        exprs = self.gen_checks(params)
        if texts:
            w("    const sv_allocator_t* a = long_allocator(vm);")
        for i in texts:
            w(f"    char* a{i} = long_str_to_c(long_as_str(args[{i}]), a);")
        if len(texts) == 1:
            w(f"    if (a{texts[0]} == NULL)")
            w("        return long_value_oom();")
        elif texts:
            w(f"    if ({' || '.join(f'a{i} == NULL' for i in texts)}) {{")
            for i in texts:
                w(f"        long_free(a, a{i});")
            w("        return long_value_oom();")
            w("    }")

        call = f"{name}({', '.join(exprs)})"
        if ret == "void":
            w(f"    {call};")
            result = "long_value_nil()"
        elif k == "num":
            result = f"long_value_number({call})"
        elif k == "bool":
            result = f"long_value_bool({call})"
        elif k == "text":
            result = f"from_text({call}, {alloc})"
        else:
            result = f"wrap_{self.base(ret)}({call}, {alloc})"
        if texts:
            if ret != "void":
                w(f"    value_t ret = {result};")
            for i in texts:
                w(f"    long_free(a, a{i});")
            w("    return ret;" if ret != "void" else "    return long_value_nil();")
        else:
            w(f"    return {result};")
        w("}")
        w()

    def gen(self):
        w = self.w
        w("/**")
        w(" * raylib for long. Generated by scripts/gen_raylib.py from raylib.h, do not edit.")
        w(" */")
        w('#include "long.h"')
        w("#include <limits.h>")
        w("#include <math.h>")
        w("#include <raylib.h>")
        w("#include <stdio.h>")
        w("#include <string.h>")
        w()
        w("typedef enum {")
        for s in self.structs.values():
            w(f"    {s.tag},")
        w("} type_tag_t;")
        w()
        w("// Hashes a double the same way long hashes numbers.")
        w("static uint32_t number_hash(double n)")
        w("{")
        w("    if (n >= (double)INT64_MIN && n < -(double)INT64_MIN && n == (double)(int64_t)n)")
        w("        return (uint32_t)(int64_t)n;")
        w()
        w("    int exp;")
        w("    n = frexp(n, &exp) * -(double)INT_MIN;")
        w("    if (!(n >= (double)INT64_MIN && n < -(double)INT64_MIN))")
        w("        return 0;")
        w("    return (uint32_t)exp + (uint32_t)(int64_t)n;")
        w("}")
        w()
        w("// Where the next snprintf writes after n characters, and how much room it has.")
        w("static char* at(char* buf, size_t size, int n)")
        w("{")
        w("    return n >= 0 && (size_t)n < size ? buf + n : buf;")
        w("}")
        w()
        w("static size_t rest(size_t size, int n)")
        w("{")
        w("    return n >= 0 && (size_t)n < size ? size - (size_t)n : 0;")
        w("}")
        w()
        w("static value_t from_text(const char* text, const sv_allocator_t* a)")
        w("{")
        w("    return text == NULL ? long_value_nil() : long_value_str(text, a);")
        w("}")
        w()
        w("static value_t to_string(const char* text, int n, const vm_ctx_t* ctx)")
        w("{")
        w("    return n < 0 ? long_value_nil() : long_value_str(text, long_ctx_allocator(ctx));")
        w("}")
        w()
        for s in self.structs.values():
            w(f"static const user_value_vtable_t {s.name}_vtable;")
        w()
        for s in self.structs.values():
            self.gen_struct(s)

        kept = []
        missing = []
        for ret, name, params in self.functions:
            if any(t == "..." for t, _ in params) or not self.supported(ret, params):
                continue
            if name not in self.defined:
                missing.append(name)
                continue
            self.gen_function(ret, name, params)
            kept.append((name, len(params)))

        for e in self.enums:
            w(f"static value_t k_{e}(const value_t* args, uint8_t n, vm_t* vm)")
            w("{")
            w("    (void)args; (void)n; (void)vm;")
            w(f"    return long_value_number({e});")
            w("}")
            w()
        for c in self.colors:
            w(f"static value_t k_{c}(const value_t* args, uint8_t n, vm_t* vm)")
            w("{")
            w("    (void)args; (void)n;")
            w(f"    return wrap_Color({c}, long_allocator(vm));")
            w("}")
            w()

        entries = []
        for s in self.structs.values():
            if any(f.dim is not None for f in s.fields):
                continue
            entries.append((f"c_{s.name}", len(s.fields), s.name))
            for alias, base in self.aliases.items():
                if self.base(alias) == s.name:
                    entries.append((f"c_{s.name}", len(s.fields), alias))
        entries += [(f"f_{name}", arity, name) for name, arity in kept]
        entries += [(f"k_{e}", 0, e) for e in self.enums + self.colors]
        names = [e[2] for e in entries]
        assert len(names) == len(set(names)), "duplicate native names"

        w("static native_fn_t fns[] = {")
        for fn, arity, name in entries:
            w(f'    {{ {fn}, {arity}, "{name}" }},')
        w("};")
        w()
        w('static const native_module_t module = { "raylib", fns, sizeof(fns) / sizeof(fns[0]) };')
        w()
        w("const native_module_t* long_module_init(void)")
        w("{")
        w("    return &module;")
        w("}")
        print(f"{len(kept)} functions, {len(self.structs)} structs, {len(self.enums)} enum values, "
              f"{len(self.colors)} colors", file=sys.stderr)
        if missing:
            print(f"Not in the library: {', '.join(missing)}", file=sys.stderr)
        return "\n".join(self.out) + "\n"


if __name__ == "__main__":
    with open(sys.argv[1]) as f:
        sys.stdout.write(Gen(f.read(), defined_functions(sys.argv[2])).gen())
