#pragma once

#include <QHash>
#include <QtDebug>
#include <typeinfo>

// Lightweight typed service locator.
//
// Usage (in MainWindow::setupServices):
//   m_registry = new ServiceRegistry();
//   m_registry->registerService(m_db);
//   m_registry->registerService(m_tracks);
//
// Usage (in views/services):
//   auto* db = registry->get<Database>();
//
// All registered pointers must outlive the ServiceRegistry.
class ServiceRegistry
{
public:
    template<typename T>
    void registerService(T* service)
    {
        m_services[typeid(T).hash_code()] = static_cast<void*>(service);
    }

    template<typename T>
    T* get() const
    {
        auto it = m_services.find(typeid(T).hash_code());
        if (it == m_services.end()) {
            qFatal("ServiceRegistry: service not registered: %s", typeid(T).name());
            return nullptr;
        }
        return static_cast<T*>(it.value());
    }

    template<typename T>
    bool has() const
    {
        return m_services.contains(typeid(T).hash_code());
    }

private:
    QHash<size_t, void*> m_services;
};
