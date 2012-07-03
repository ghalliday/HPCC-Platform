/*##############################################################################

    Copyright (C) 2012 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

layout := { unsigned1 id };
layoutRedirect := layout;

mymod := module
  export Model := interface
    export dataset(layout) ds;
  end;

EXPORT Instance1(dataset(layout) inDs = dataset([], layoutRedirect)) := module(Model)
    export dataset(layout) ds := inDs; // works
  end;

EXPORT Instance2(dataset(layout) inDs = dataset([], layoutRedirect)) := module(Model)
    // Error: Explicit type for ds doesn't match definition in base module
    // I expected this to work since layout and layoutRedirect are the same.
     export dataset(layoutRedirect) ds := inDs;
  end;

EXPORT Instance3(dataset(layout) inDs = dataset([], layoutRedirect)) := module(Model)
    // Error: syntax error near ":=" : expected datarow, identifier, ...
    // I expected this to be the same as 2nd working example.
    // This example is unrelated to using layoutRedirect.

   export ds := inDs;

  end;
end;

output(mymod.Instance1().ds);

output(mymod.Instance2().ds);

output(mymod.Instance3().ds);
