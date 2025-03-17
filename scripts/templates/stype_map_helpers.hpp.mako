<%!
import os
import re
from templates import helper as th
%><%
    n=namespace
    N=n.upper()
    x=tags['$x']
    X=x.upper()
%>
// This file is autogenerated from the template at ${os.path.dirname(self.template.filename)}/${os.path.basename(self.template.filename)}

%for obj in th.extract_objs(specs, r"enum"):
 %if obj["name"] == '$x_structure_type_t':
  %for etor in th.get_etors(obj):
   %if 'UINT32' not in etor['name']:
template <>
struct stype_map<${x}_${etor['desc'][3:]}> : stype_map_impl<${X}_${etor['name'][3:]}> {};
   %endif
  %endfor
 %endif
%endfor

