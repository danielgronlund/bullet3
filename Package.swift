// swift-tools-version: 5.9

import PackageDescription

let package = Package(
  name: "Bullet",
  products: [
    .library(
      name: "Bullet",
      targets: ["Bullet"]),
  ],
  targets: [
    .target(
      name: "Bullet",
      path: "src",
      exclude: [
        "btBulletCollisionAll.cpp",
        "btBulletDynamicsAll.cpp",
        "btLinearMathAll.cpp"
      ],
      sources: ["."]
    ),
  ]
)

