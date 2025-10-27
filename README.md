# agea
AGEA - Awesome Game Engine AGEA

# Stages of type loading 
 - Make glue type ids 
 - Make type resolver
 - Handle packages in topoorder
    - Create Model types. For every type in topoorder
        - Create RT type
            - Assign type handers
            - Assing architecture
            - Assign parent
    - Create types  properties. For every type in topoorder (Why separate? We need properties needs to have all tipes. We don't want to create type->property dependancy)
        - Create properties
        - Inherit properties 
    - Create Render. For every type in topoorder
        - Assign render types overrides
        - Assign render properties
    - Finalize type. Inherit handlers and architecture