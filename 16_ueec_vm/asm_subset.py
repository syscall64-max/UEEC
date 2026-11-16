"""Strict encoder for the supplied data-only NASM fixture subset.

Supports BITS, %define (including expression macros), labels, db/dw/dd/dq.
No native assembler, Python eval, arbitrary code, includes or external tools.
NASM may be used instead to cross-check reference/*.asm independently.
"""
import ast
import operator
import re
from pathlib import Path

OPS = {ast.Add:operator.add, ast.Sub:operator.sub, ast.Mult:operator.mul,
       ast.LShift:operator.lshift, ast.RShift:operator.rshift,
       ast.BitOr:operator.or_, ast.BitAnd:operator.and_, ast.BitXor:operator.xor}

def assemble(path):
    source=Path(path).read_text()
    # Join NASM continuation lines before removing comments.
    source=re.sub(r'\\\s*\n',' ',source)
    defines={}; macros={}; labels={}; records=[]; cursor=0
    for raw in source.splitlines():
        line=raw.split(';',1)[0].strip()
        if not line or re.fullmatch(r'\[?BITS\s+64\]?',line,re.I): continue
        if line.startswith('%define '):
            m=re.fullmatch(r'%define\s+(\w+)(?:\(([^)]*)\))?\s+(.+)',line)
            if not m: raise ValueError(line)
            if m[2] is not None: macros[m[1]]=([v.strip() for v in m[2].split(',')],m[3])
            else: defines[m[1]]=m[3]
            continue
        if line.endswith(':'):
            name=line[:-1]
            if not re.fullmatch(r'\w+',name) or name in labels: raise ValueError(line)
            labels[name]=cursor; continue
        m=re.fullmatch(r'(db|dw|dd|dq)\s+(.+)',line,re.I)
        if not m: raise ValueError('Unsupported fixture syntax: '+line)
        # All supplied lines have one expression. No string/data splitting.
        width={'db':1,'dw':2,'dd':4,'dq':8}[m[1].lower()]
        records.append((width,m[2])); cursor+=width

    def expression(text, local=None, depth=0):
        if depth>64: raise ValueError('Expression expansion depth')
        local=local or {}
        def visit(n):
            if isinstance(n,ast.Constant) and type(n.value) is int:return n.value
            if isinstance(n,ast.Name):
                if n.id in local:return local[n.id]
                if n.id in labels:return labels[n.id]
                if n.id in defines:return expression(defines[n.id],local,depth+1)
            if isinstance(n,ast.BinOp):
                a,b=visit(n.left),visit(n.right)
                if isinstance(n.op,ast.Div):
                    if not b or a%b:raise ValueError('Non-integral fixture division')
                    return a//b
                if type(n.op) in OPS:return OPS[type(n.op)](a,b)
            if isinstance(n,ast.UnaryOp):
                v=visit(n.operand)
                if isinstance(n.op,ast.USub):return -v
                if isinstance(n.op,ast.UAdd):return v
                if isinstance(n.op,ast.Invert):return ~v
            if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id in macros:
                params,body=macros[n.func.id]
                if n.keywords or len(params)!=len(n.args):raise ValueError('Macro arguments')
                return expression(body,dict(zip(params,map(visit,n.args))),depth+1)
            raise ValueError('Unsupported fixture expression: '+ast.dump(n))
        return visit(ast.parse(text,mode='eval').body)
    out=bytearray()
    for width,text in records:
        value=expression(text); bits=width*8
        if not -(1<<(bits-1))<=value<(1<<bits):raise ValueError('Fixture value overflow')
        out.extend((value&((1<<bits)-1)).to_bytes(width,'little'))
    return bytes(out)

if __name__=='__main__':
    import sys
    Path(sys.argv[2]).write_bytes(assemble(sys.argv[1]))
