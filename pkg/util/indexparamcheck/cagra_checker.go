package indexparamcheck

import (
	"fmt"
	"strconv"
)

// diskannChecker checks if an diskann index can be built.
type cagraChecker struct {
	floatVectorBaseChecker
}

func (c *cagraChecker) CheckTrain(params map[string]string) error {
	if err := c.baseChecker.CheckTrain(params); err != nil {
		return err
	}

	interDegreeStr, interDegreeExist := params[CAGRA_INTER_DEGREE]
	if !interDegreeExist {
		return fmt.Errorf("cagra inter degree not found")
	}
	graphDegreeStr, graphDegreeExist := params[CAGRA_GRAPH_DEGREE]
	if !graphDegreeExist {
		return fmt.Errorf("cagra graph degree not found")
	}
	interDegree, err := strconv.Atoi(interDegreeStr)
	if err != nil {
		return fmt.Errorf("invalid cagra inter degree: %s", interDegreeStr)
	}

	graphDegree, err := strconv.Atoi(graphDegreeStr)
	if err != nil {
		return fmt.Errorf("invalid cagra graph degree: %s", graphDegreeStr)
	}

	if interDegree < graphDegree {

		return fmt.Errorf("Graph degree cannot be larger than intermediate graph degree")
	}

	if !CheckStrByValues(params, Metric, CagraMetrics) {
		return fmt.Errorf("metric type not found or not supported, supported: %v", CagraMetrics)
	}

	return nil

}

func (c cagraChecker) StaticCheck(params map[string]string) error {
	return c.staticCheck(params)
}

func newCagraChecker() IndexChecker {
	return &cagraChecker{}
}
