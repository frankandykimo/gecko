// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Throws a TypeError if `this` is a Map object.
info: >
  WeakMap.prototype.get ( key )

  ...
  3. If M does not have a [[WeakMapData]] internal slot, throw a TypeError
  exception.
  ...
features: [Map]
---*/

assert.throws(TypeError, function() {
  WeakMap.prototype.get.call(new Map(), 1);
});

assert.throws(TypeError, function() {
  var map = new WeakMap();
  map.get.call(new Map(), 1);
});

reportCompare(0, 0);
