export module crumb.assets;
import crumb.type;

export namespace crumb::assets {

// A read-only view over bytes embedded in the executable at build time.
struct Asset {
  const uchar* data = nullptr;
  size_t       size = 0;

  bool Empty() const { return data == nullptr || size == 0; }
};

Asset FontPng();
Asset IconPng();
Asset LspStub();

} // namespace crumb::assets
