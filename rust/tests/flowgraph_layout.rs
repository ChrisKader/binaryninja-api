use binaryninja::binary_view::BinaryViewExt;
use binaryninja::flowgraph::layout::{register_flowgraph_layout, FlowGraphLayout};
use binaryninja::flowgraph::{FlowGraph, FlowGraphNode};
use binaryninja::function::FunctionViewType;
use binaryninja::headless::Session;
use std::path::PathBuf;
use std::time::Duration;

struct FlowGraphLayoutTest {}

impl FlowGraphLayout for FlowGraphLayoutTest {
    fn layout(&self, graph: &FlowGraph, nodes: &[FlowGraphNode]) -> bool {
        todo!()
    }
}

#[test]
fn test_flowgraph_layout() {
    let _session = Session::new().expect("Failed to initialize session");
    let out_dir = env!("OUT_DIR").parse::<PathBuf>().unwrap();
    let view = binaryninja::load(out_dir.join("atox.obj")).expect("Failed to create view");

    let function = view.entry_point_function().expect("Entry point exists");
    let graph = function.create_graph(FunctionViewType::MediumLevelIL, None);
    assert!(
        graph.request_layout_and_wait(Duration::from_secs(5)),
        "Took too long to create graph"
    );
    assert!(
        graph.is_layout_complete(),
        "Should always be true if request_layout_and_wait returned true"
    );
    assert!(graph.nodes().len() > 0, "Graph should have nodes");

    let (_, layout) = register_flowgraph_layout("test", FlowGraphLayoutTest {});
    let other_function = view
        .function_at(&view.default_platform().unwrap(), 0x3b440)
        .unwrap();
    let graph = other_function.create_graph_immediate(FunctionViewType::MediumLevelIL, None);
    let nodes = graph
        .nodes()
        .iter()
        .map(|n| n.to_owned())
        .collect::<Vec<_>>();
    layout.layout(&graph, nodes);
}
