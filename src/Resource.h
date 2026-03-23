// #ifndef RESOURCE_H
// #define RESOURCE_H

// #include <string>

// namespace Expectre {
// // Base class for resources that are transferred between CPU and GPU

// class Resource {
// public:
//   explicit Resource(const std::string &id) : resource_id(id) {}
//   virtual ~Resource() = default;

//   const std::string &get_id() const { return resource_id; };
//   bool is_loaded() const { return m_loaded; }

//   // Virtual interface for resource-specific loaded and unloading

//   bool load_to_GPU() {
//     m_loaded = load();
//     return m_loaded;
//   }

//   void unload_from_GPU() {
//     unload();
//     m_loaded = false;
//   }

// protected:
//   virtual bool load() = 0;
//   virtual bool unload() = 0;
//   std::string resource_id; // Unique identifier for this resource
//   bool m_loaded = false;   // State flag whether resource has been loaded to GPU

// private:
// };
// } // namespace Expectre
// #endif // RESOURCE_H